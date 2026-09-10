#pragma once
#include <filesystem>
#include <string>

namespace helpers {
float roundFloat(float x, int num_decimal_precision_digits);
std::string trim(std::string &str);

enum class pathTypeEnum { FILE, DIRECTORY, INVALID };
pathTypeEnum pathType(const std::filesystem::path &path);

std::string readFileFirstLine(std::filesystem::path pathToFile);
} // namespace helpers
