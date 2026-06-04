#include "Device.hpp"
#include "EnergySensor.hpp"
#include "Sensor.hpp"
#include "SensorTypes.hpp"
#include "SensorWhitelist.hpp"
#include "vectorFind.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <ostream>
#include <print>
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
  std::print("unable to find type {} for sensor {}\n", parsedPart1, this->name);
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
    if (!IsInVector<string>(extensions, "input") && !IsInVector<string>(extensions, "average")) {
      continue;
    }

    string label = sensorBase;
    string valueSrc;
    SensorType type = DeduceSensorType(sensorBase);

    if (IsInVector(extensions, std::string("input"))) {
      valueSrc = path / (sensorBase + "_input");
    } else if (IsInVector<string>(extensions, "average")) {
      valueSrc = path / (sensorBase + "_average");
    }
    if (IsInVector<string>(extensions, "label")) {
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
  for (auto &sensor : Sensors) {
    j["sensors"] += sensor->Serialize();
  }
  return j;
}


// TEMPORARY, TO BE REMOVED
// void Device::Display() {
//   for (auto &a : Sensors) {
//     a->updateValue();
//     auto read = a->getReadings();
//     if (std::isnan(read.value))
//       return;

//     std::print(" val: {}\n min: {}\n max: {}\n avg: {}\n", read.value, read.min_value,
//                read.max_value, read.sum / read.times);
//   }
// }
