#pragma once
#include <filesystem>

#include "../hwmon.hpp"

struct SharedHwmonParser {
  SharedHwmonParser() = default;
  ~SharedHwmonParser() = default;


  static hwmon::AvailableSensorsMap parseHwmonDirectory(const hwmon::fs::path &path);

  static void createSensors(const hwmon::fs::path &path,
                            const hwmon::AvailableSensorsMap &available_sensors,
                            hwmon::SensorVec &sensors);

  static hwmon::SensorVec returnSensors(const hwmon::fs::path &path,
                                        const hwmon::AvailableSensorsMap &available_sensors);
};
