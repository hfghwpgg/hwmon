#pragma once
#include "Sensor.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace hwmon {
namespace fs = std::filesystem;
using AvailableSensorsMap = std::unordered_map<std::string, std::vector<std::string>>;
using SensorVec = std::vector<std::unique_ptr<Sensor>>;
} // namespace hwmon
