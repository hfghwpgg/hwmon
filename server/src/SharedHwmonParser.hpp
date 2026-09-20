#pragma once
#include <filesystem>

#include "helpers.hpp"

struct SharedHwmonParser {
  SharedHwmonParser() = default;
  ~SharedHwmonParser() = default;


  static helpers::AvailableSensorsMap parseHwmonDirectory(const helpers::fs::path &path);

  static void createSensors(const helpers::fs::path &path,
                            const helpers::AvailableSensorsMap &available_sensors,
                            helpers::SensorVec &sensors);

  static helpers::SensorVec returnSensors(const helpers::fs::path &path,
                                          const helpers::AvailableSensorsMap &available_sensors);
};
