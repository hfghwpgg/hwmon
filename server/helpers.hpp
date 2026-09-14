#pragma once
#include <filesystem>
#include <string>

#include "hwmon.hpp"

namespace helpers {
float roundFloat(float x, int num_decimal_precision_digits);
std::string trim(std::string &str);

enum class pathTypeEnum { FILE, DIRECTORY, INVALID };
pathTypeEnum pathType(const hwmon::fs::path &path);

std::string readFileFirstLine(hwmon::fs::path pathToFile);
} // namespace helpers
