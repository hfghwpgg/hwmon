#include "../Device.hpp"
#include "GeneralDevice.hpp"

#include <fmt/format.h>
#include <istream>
#include <nlohmann/json.hpp>
#include <stddef.h>
#include <spdlog/spdlog.h>
#include <fmt/base.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <utility>

#include "EnergySensor.hpp"
#include "Sensor.hpp"
#include "SensorType.hpp"
#include "SensorWhitelist.hpp"
#include "helpers.hpp"

enum class DeviceType;

using std::string;
using std::unordered_map;
using std::vector;

GeneralDevice::GeneralDevice(string name, DeviceType type, fs::path path) :
    Device(name, type),
    path(path) {
  spdlog::debug("CURRENT GENERAL DEVICE: {} <{}>", path.string(), name);
  if (helpers::pathType(path) != helpers::pathTypeEnum::DIRECTORY) {
    spdlog::critical("invalid path for device {}: {}", name, path.string());
    throw std::runtime_error("invalid path, check logs");
  };
}

#ifdef DEBUG
GeneralDevice::~GeneralDevice() {
  spdlog::debug("GeneralDevice destroyed: {}", name);
}
#endif

void GeneralDevice::initialize() {
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

void GeneralDevice::createSensors(unordered_map<string, vector<string>> availableSensors) {
  for (const auto &[sensorBase, extensions] : availableSensors) {
    bool hasInput = false;
    bool hasAverage = false;
    fs::path valueSrcPath;
    string label = sensorBase;
    for (const auto &ext : extensions) {
      if (ext == "input") {
        hasInput = true;
        valueSrcPath = path / (sensorBase + "_input");
      }
      if (ext == "average") {
        hasAverage = true;
        valueSrcPath = path / (sensorBase + "_average");
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
      spdlog::error("singular sensor has both input and average fields, please make a report "
                    "on this. ignoring this sensor");
      continue;
    }

    SensorType type = helpers::deduceSensorType(sensorBase);
    if (type == SensorType::UNKNOWN) {
      spdlog::warn("unable to find type {} for sensor", sensorBase);
    }

    auto valueSrc_ptr = std::make_unique<std::ifstream>(valueSrcPath);
    if (!valueSrc_ptr->is_open()) {
      spdlog::critical("unable to open file {}\n", valueSrcPath.string());
      throw std::runtime_error(std::format("unable to open file {}\n", valueSrcPath));
    };

    if (type == SensorType::ENERGY) {
      sensors.emplace_back(std::make_unique<EnergySensor>(std::move(valueSrc_ptr), label, type));
    } else {
      sensors.emplace_back(std::make_unique<Sensor>(std::move(valueSrc_ptr), label, type));
    }
  }
}
