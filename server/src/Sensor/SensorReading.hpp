#pragma once
#include <cstdint>
#include <nlohmann/json_fwd.hpp>

#define MAX_ERRORS 3

struct SensorReading {
  double value;
  double min_value;
  double max_value;
  double sum;
  size_t times;
  // this should reach 3, then sensor
  // prints out error message and increments
  // it to 4
  uint8_t errorCount;

  nlohmann::json serialize();
  void reset();
};
