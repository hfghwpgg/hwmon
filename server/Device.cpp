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
    Sensors() {
  Sensors.reserve(10);
  Initialize();
}

#ifdef DEBUG
Device::~Device() {
  spdlog::debug("Device destroyed: {}", this->name);
}
#endif

SensorType Device::DeduceSensorType(string parsedPart1) {
  auto lastNonDigit = parsedPart1.find_last_not_of("0123456789");

  std::string_view prefix;
  if (lastNonDigit != string::npos) {
    prefix = std::string_view(parsedPart1).substr(0, lastNonDigit + 1);
  } else {
    prefix = parsedPart1;
  }
  auto it = SensorConfigMap.find(prefix);
  if (it != SensorConfigMap.end()) {
    return it->second;
  }
  spdlog::warn("unable to find type {} for sensor {}", parsedPart1, this->name);
  return SensorType::UNKNOWN;
}

void Device::Initialize() {
  unordered_map<string, vector<string>> available_sensors;
  for (const auto &entry : fs::directory_iterator(path)) {
    if (entry.is_regular_file()) {
      // stem returns filename
      // without extension
      string filename = entry.path().stem();
      size_t underscorePos = filename.find('_');
      if (underscorePos != string::npos) {
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
    }
  }
  CreateSensors(available_sensors);
}

void Device::CreateSensors(unordered_map<string, vector<string>> availableSensors) {
  for (const auto &[sensorBase, extensions] : availableSensors) {
    bool hasInput = false;
    bool hasAverage = false;
    bool hasLabel = false;
    for (const auto &ext : extensions) {
      if (ext == "input")
        hasInput = true;
      if (ext == "average")
        hasAverage = true;
      if (ext == "label")
        hasLabel = true;
    }
    // if no reading available, continue
    if (hasInput == false && hasAverage == false) {
      spdlog::warn("sensor {} exposes no known reading interface", sensorBase);
      continue;
    }

    string label = sensorBase;
    string valueSrc;
    SensorType type = DeduceSensorType(sensorBase);

    if (hasInput) {
      valueSrc = path / (sensorBase + "_input");
    } else if (hasAverage) {
      valueSrc = path / (sensorBase + "_average");
    }
    if (hasLabel) {
      string temp = path / (sensorBase + "_label");
      std::ifstream f{temp};
      f.clear();
      f.seekg(0);
      std::getline(f, label);
      f.close();
    }


    if (type == SensorType::ENERGY) {
      Sensors.emplace_back(std::make_unique<EnergySensor>(valueSrc, label, type));
    } else {
      Sensors.emplace_back(std::make_unique<Sensor>(valueSrc, label, type));
    }
  }
}

void Device::Read() {
  for (auto &sensor : Sensors) {
    sensor->UpdateValue();
  }
}

nlohmann::json Device::Serialize() {
  nlohmann::json j;
  j["name"] = this->name;
  j["type"] = this->type;
  for (auto &sensor : Sensors) {
    j["sensors"] += sensor->Serialize();
  }
  return j;
}
