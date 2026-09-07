#include "helpers.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>

#include "SensorType.hpp"

namespace helpers {
float roundFloat(float x, int num_decimal_precision_digits) {
  float power_of_10 = std::pow(10, num_decimal_precision_digits);
  return std::round(x * power_of_10) / power_of_10;
}

std::string trim(std::string &str) {
  str.erase(str.find_last_not_of(' ') + 1); // Suffixing spaces
  str.erase(0, str.find_first_not_of(' ')); // Prefixing spaces
  return str;
}

SensorType deduceSensorType(std::string sensorName) {
  auto lastDigit = sensorName.find_first_of("0123456789");

  std::string_view prefix;
  if (lastDigit != std::string::npos) {
    prefix = std::string_view(sensorName).substr(0, lastDigit);
  } else {
    prefix = sensorName;
  }
  auto it = sensorConfigMap.find(prefix);
  if (it != sensorConfigMap.end()) {
    return it->second;
  }
  // fallback
  return SensorType::UNKNOWN;
}

pathTypeEnum pathType(const std::filesystem::path &path) {
  struct stat sb; // struct for metadata
  if (stat(path.c_str(), &sb) == 0) {
    // S_IFDIR = 1 => directory
    return (sb.st_mode & S_IFDIR) ? pathTypeEnum::DIRECTORY : pathTypeEnum::FILE;
  }
  return pathTypeEnum::INVALID;
}

std::string readFileFirstLine(std::filesystem::path pathToFile) {
  std::string content;
  std::ifstream f{pathToFile};
  f.clear();
  f.seekg(0);
  std::getline(f, content);
  f.close();
  return content;
}
} // namespace helpers
