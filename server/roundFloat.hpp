#pragma once
#include <cmath>

static float roundFloat(float x, int num_decimal_precision_digits) {
  float power_of_10 = std::pow(10, num_decimal_precision_digits);
  return std::round(x * power_of_10) / power_of_10;
}
