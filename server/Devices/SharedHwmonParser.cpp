
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "../EnergySensor.hpp"
#include "../SensorWhitelist.hpp"
#include "../helpers.hpp"
#include "SharedHwmonParser.hpp"
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

std::unordered_map<std::string, std::vector<std::string>>
SharedHwmonParser::parseHwmonDirectory(const std::filesystem::path &path) {
  std::unordered_map<std::string, std::vector<std::string>> available_sensors;
  for (const auto &entry : fs::directory_iterator(path)) {
    if (!entry.is_regular_file()) {
      spdlog::debug("{} is not a regular file", entry.path().string());
      continue;
    }
    // stem returns filename
    // without extension
    std::string filename = entry.path().stem();

    // usually sensors contain underscores
    // not all tho
    // TODO: include sensors that DO NOT contain
    // underscores (ex: pwm sensors dont)
    size_t underscorePos = filename.find('_');
    if (underscorePos == std::string::npos) {
      spdlog::debug("{} does not contain an underscore", filename);
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
void SharedHwmonParser::createSensors(
    const fs::path &path,
    const std::unordered_map<std::string, std::vector<std::string>> &availableSensors,
    std::vector<std::unique_ptr<Sensor>> &sensors) {
  for (const auto &[sensorBase, extensions] : availableSensors) {
    bool hasInput = false;
    bool hasAverage = false;
    fs::path valueSrcPath;
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
    // if no reading available, continue
    if (!hasInput && !hasAverage) {
      spdlog::warn("sensor {} exposes no known reading interface", sensorBase);
      continue;
    } else if (hasInput && hasAverage) {
      spdlog::error("singular sensor has both input and average fields, please make a report "
                    "on this. ignoring this sensor");
      continue;
    }

    const SensorType type = helpers::deduceSensorType(sensorBase);
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
