#pragma once
#include <filesystem>

#include "helpers.hpp"

struct SharedHwmonParser {
  SharedHwmonParser() = default;
  ~SharedHwmonParser() = default;


  static helpers::AvailableSensorsMap parseHwmonDirectory(const std::filesystem::path &path);

  static void createSensors(const std::filesystem::path &path,
                            const helpers::AvailableSensorsMap &availableSensors,
                            helpers::SensorVec &sensors);

  static helpers::SensorVec returnSensors(const std::filesystem::path &path,
                                          const helpers::AvailableSensorsMap &availableSensors);
};
