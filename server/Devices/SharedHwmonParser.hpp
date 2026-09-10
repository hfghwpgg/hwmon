#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Sensor.hpp"

struct SharedHwmonParser {
  SharedHwmonParser() = default;
  ~SharedHwmonParser() = default;


  static auto parseHwmonDirectory(const std::filesystem::path &path)
      -> std::unordered_map<std::string, std::vector<std::string>>;

  static std::vector<std::unique_ptr<Sensor>>
  createSensors(const std::filesystem::path &path,
                const std::unordered_map<std::string, std::vector<std::string>> &available_sensors);
};
