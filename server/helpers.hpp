#pragma once
#include <cmath>
#include <string>
#include <algorithm>
#include <vector>
#include <string>
#include "SensorType.hpp"

namespace helpers {
static inline float roundFloat(float x, int num_decimal_precision_digits) {
  float power_of_10 = std::pow(10, num_decimal_precision_digits);
  return std::round(x * power_of_10) / power_of_10;
}

static inline std::string trim(std::string &str) {
  str.erase(str.find_last_not_of(' ') + 1); // Suffixing spaces
  str.erase(0, str.find_first_not_of(' ')); // Prefixing spaces
  return str;
}

template <typename T> static inline bool isInVector(const std::vector<T> &vec, const T &thing) {
  return (std::find(vec.begin(), vec.end(), thing) != vec.end());
}

static inline SensorType deduceSensorType(std::string parsedPart1) {
  auto lastNonDigit = parsedPart1.find_last_not_of("0123456789");

  std::string_view prefix;
  if (lastNonDigit != std::string::npos) {
    prefix = std::string_view(parsedPart1).substr(0, lastNonDigit + 1);
  } else {
    prefix = parsedPart1;
  }
  auto it = sensorConfigMap.find(prefix);
  if (it != sensorConfigMap.end()) {
    return it->second;
  }
  // fallback
  return SensorType::UNKNOWN;
}
} // namespace helpers
