#include "Device.hpp"
#include "EnergySensor.hpp"
#include "Sensor.hpp"
#include "SensorTypes.hpp"
#include "SensorWhitelist.hpp"
#include "vectorFind.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using std::format;
using std::string;
using std::unordered_map;
using std::vector;

Device::Device(fs::path path, string name) :
    path(path),
    name(name),
    Sensors() {
  Sensors.reserve(10);
  Initialize();
}

Device::~Device() {
  std::cout << "Device destroyed" << std::endl;
}

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
  std::cout << format("unable to find type {} for sensor {}", parsedPart1, this->name);
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

        if (!isWhitelistedSensorAttribute(part2))
          continue;

        if (available_sensors.count(part1)) {
          available_sensors.at(part1).push_back(part2);
        } else {
          available_sensors.insert({part1, vector<string>{part2}});
        }
      }
    }
  }
  createSensors(available_sensors);
}

void Device::createSensors(unordered_map<string, vector<string>> available_sensors) {
  for (const auto &[sensorBase, extensions] : available_sensors) {
    if (!isInVector<string>(extensions, "input") && !isInVector<string>(extensions, "average")) {
      continue;
    }

    string label = sensorBase;
    string valueSrc;
    SensorType type = DeduceSensorType(sensorBase);

    if (isInVector(extensions, std::string("input"))) {
      valueSrc = path / (sensorBase + "_input");
    } else if (isInVector<string>(extensions, "average")) {
      valueSrc = path / (sensorBase + "_average");
    }
    if (isInVector<string>(extensions, "label")) {
      label = path / (sensorBase + "_label");
    }


    if (type == SensorType::ENERGY) {
      Sensors.emplace_back(std::make_unique<EnergySensor>(valueSrc, label, type));
    } else {
      Sensors.emplace_back(std::make_unique<Sensor>(valueSrc, label, type));
    }
  }
}

// void Device::Read() {
//     for (auto a : available_sensors) {
//         std::cout << a.first << std::endl;
//         for (string v : a.second)
//         {
//             std::cout << v << ',';
//         }
//         std::cout << std::endl;
//     }
// }

void Device::Read() {
  for (auto &a : Sensors) {
    a->updateValue();
    a->getReadings();
  }
}

// TEMPORARY, TO BE REMOVED
#include <format>
void Device::Display() {
  for (auto &a : Sensors) {
    a->updateValue();
    auto read = a->getReadings();
    if (std::isnan(read.value))
      return;

    std::cout << std::format(" val: {}\n min: {}\n max: {}\n avg: {}\n", read.value, read.min_value,
                             read.max_value, read.sum / read.times);
  }
}