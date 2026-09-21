#include <filesystem>
#include <fstream>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "Sensor/Sensor.hpp"
#include "Sensor/SensorType.hpp"
#include "Sensor/SensorWhitelist.hpp"
#include "SharedHwmonParser.hpp"
#include "helpers.hpp"

helpers::AvailableSensorsMap
SharedHwmonParser::parseHwmonDirectory(const std::filesystem::path &path) {
  helpers::AvailableSensorsMap available_sensors;
  for (const auto &entry : std::filesystem::directory_iterator(path)) {
    if (!entry.is_regular_file()) {
      spdlog::trace("{} is not a regular file", entry.path().string());
      continue;
    }
    // stem returns filename
    // without extension
    std::string filename = entry.path().stem();

    // usually sensors contain underscores
    // not all tho
    // TODO: include sensors that DO NOT contain
    // underscores (ex: pwm sensors dont)

    // // pwm readings are weird, will work on it later
    // // uncommenting this block will allow them to be read
    // if (filename.contains("pwm")) {
    //   available_sensors.insert({filename, {""}}); // pwm sensors dont have
    //   continue;                                   // any extensions
    // }

    size_t underscorePos = filename.find('_');
    if (underscorePos == std::string::npos) {
      spdlog::trace("{} does not contain an underscore", filename);
      continue;
    }

    std::string part1 = filename.substr(0, underscorePos);
    std::string part2 = filename.substr(underscorePos + 1);

    if (!IsWhitelistedSensorAttribute(part2))
      continue;

    if (available_sensors.contains(part1)) {
      available_sensors.at(part1).push_back(part2);
    } else {
      available_sensors.insert({part1, std::vector<std::string>{part2}});
    }
  }

  return available_sensors;
}
void SharedHwmonParser::createSensors(const std::filesystem::path &path,
                                      const helpers::AvailableSensorsMap &availableSensors,
                                      helpers::SensorVec &sensors) {
  for (const auto &[sensorBase, extensions] : availableSensors) {
    bool isPwm = sensorBase.contains("pwm");
    bool hasInput = false;
    bool hasAverage = false;

    std::filesystem::path valueSrcPath;
    std::string label = sensorBase;
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
        label = helpers::readFileFirstLine(path / (sensorBase + "_label"));
      }
    }

    if (isPwm) {
      valueSrcPath = path / sensorBase;
    }
    // if no reading available, continue
    if (!hasInput && !hasAverage && !isPwm) {
      spdlog::warn("sensor {} exposes no known reading interface", sensorBase);
      continue;
    }
    if (hasInput && hasAverage) {
      spdlog::error("singular sensor has both input and average fields, please make a report "
                    "on this. ignoring this sensor");
      continue;
    }

    const SensorType type = Sensor::deduceSensorType(sensorBase);
    if (type == SensorType::UNKNOWN) {
      spdlog::warn("unable to find type for sensor {}", sensorBase);
    }

    if (type == SensorType::ENERGY) {
      // energy sensors return power
      Sensor::makeFileSensor<TransformDelta>(sensors, valueSrcPath, {label, SensorType::POWER});
    } else {
      Sensor::makeFileSensor<TransformScale>(sensors, valueSrcPath, {label, type});
    }
  }
}
helpers::SensorVec
SharedHwmonParser::returnSensors(const std::filesystem::path &path,
                                 const helpers::AvailableSensorsMap &available_sensors) {
  helpers::SensorVec sensors;
  createSensors(path, available_sensors, sensors);
  return sensors;
}
