#include "Device.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <stddef.h>
#include <spdlog/spdlog.h>
#include <fmt/base.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <utility>

#include "EnergySensor.hpp"
#include "Sensor.hpp"
#include "SensorType.hpp"
#include "SensorWhitelist.hpp"

enum class DeviceType;

using std::string;
using std::unordered_map;
using std::vector;

Device::Device(fs::path path, string name, DeviceType type) :
    path(path),
    name(name),
    type(type),
    sensors() {
  sensors.reserve(10);
  spdlog::debug("CURRENT DEVICE: {} <{}>", this->path.string(), this->name);
  initialize();
}

#ifdef DEBUG
Device::~Device() {
  spdlog::debug("Device destroyed: {}", this->name);
}
#endif

SensorType Device::deduceSensorType(string parsedPart1) {
  auto lastNonDigit = parsedPart1.find_last_not_of("0123456789");

  std::string_view prefix;
  if (lastNonDigit != string::npos) {
    prefix = std::string_view(parsedPart1).substr(0, lastNonDigit + 1);
  } else {
    prefix = parsedPart1;
  }
  auto it = sensorConfigMap.find(prefix);
  if (it != sensorConfigMap.end()) {
    return it->second;
  }
  // fallback
  return SensorType::UNKNOWN;
}

void Device::initialize() {
  unordered_map<string, vector<string>> available_sensors;
  for (const auto &entry : fs::directory_iterator(path)) {
    if (!entry.is_regular_file()) {
      spdlog::warn("{} is not a regular file", entry.path().string());
      continue;
    }
    // stem returns filename
    // without extension
    string filename = entry.path().stem();
    size_t underscorePos = filename.find('_');
    if (underscorePos == string::npos) {
      spdlog::warn("{} does not contain an underscore", filename);
      continue;
    }

    string part1 = filename.substr(0, underscorePos);
    string part2 = filename.substr(underscorePos + 1);

    if (!IsWhitelistedSensorAttribute(part2))
      continue;

    if (available_sensors.count(part1)) {
      available_sensors.at(part1).push_back(part2);
    } else {
      available_sensors.insert({part1, vector<string>{part2}});
    }
  }
  createSensors(available_sensors);
}

void Device::createSensors(unordered_map<string, vector<string>> availableSensors) {
  for (const auto &[sensorBase, extensions] : availableSensors) {
    bool hasInput = false;
    bool hasAverage = false;
    string valueSrc;
    string label = sensorBase;
    for (const auto &ext : extensions) {
      if (ext == "input") {
        hasInput = true;
        valueSrc = this->path / (sensorBase + "_input");
      }
      if (ext == "average") {
        hasAverage = true;
        valueSrc = this->path / (sensorBase + "_average");
      }

      if (ext == "label") {
        string temp = path / (sensorBase + "_label");
        std::ifstream f{temp};
        f.clear();
        f.seekg(0);
        std::getline(f, label);
        f.close();
      }
    }
    // if no reading available, continue
    if (!hasInput && !hasAverage) {
      spdlog::warn("sensor {} exposes no known reading interface", sensorBase);
      continue;
    } else if (hasInput && hasAverage) {
      spdlog::warn("singular sensor has both input and average fields, please report this to the "
                   "maintainer. ignoring this sensor");
      continue;
    }

    SensorType type = deduceSensorType(sensorBase);
    if (type == SensorType::UNKNOWN) {
      spdlog::warn("unable to find type {} for sensor", sensorBase);
    }

    if (type == SensorType::ENERGY) {
      sensors.emplace_back(std::make_unique<EnergySensor>(valueSrc, label, type));
    } else {
      sensors.emplace_back(std::make_unique<Sensor>(valueSrc, label, type));
    }
  }
}

void Device::read() {
  for (auto &sensor : sensors) {
    sensor->updateValue();
  }
}

nlohmann::json Device::serialize() {
  nlohmann::json j;
  j["name"] = this->name;
  j["type"] = this->type;
  for (auto &sensor : sensors) {
    j["sensors"] += sensor->serialize();
  }
  return j;
}
