#pragma once
#include <cmath>
#include <string>
#include <algorithm>
#include <vector>
#include <string>

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
} // namespace helpers
