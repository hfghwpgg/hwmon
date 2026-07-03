#pragma once
#include <nlohmann/json_fwd.hpp>
#include <stddef.h>

struct SensorReading {
  double value;
  double min_value;
  double max_value;
  long double sum;
  size_t times;

  nlohmann::json serialize();
  void reset();
};
