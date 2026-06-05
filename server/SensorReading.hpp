#pragma once
#include <nlohmann/json_fwd.hpp>
struct SensorReading {
  float value;
  float min_value;
  float max_value;
  double sum;
  size_t times;

  nlohmann::json Serialize();
};
