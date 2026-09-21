#pragma once
#include "Sensor/Sensor.hpp"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace helpers {
using AvailableSensorsMap = std::unordered_map<std::string, std::vector<std::string>>;
using SensorVec = std::vector<std::unique_ptr<Sensor>>;

float roundFloat(float x, int num_decimal_precision_digits);
std::string trim(std::string &str);

enum class pathTypeEnum { FILE, DIRECTORY, INVALID };
pathTypeEnum pathType(const std::filesystem::path &path);

std::string readFileFirstLine(std::filesystem::path pathToFile);
} // namespace helpers
