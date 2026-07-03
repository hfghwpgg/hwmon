#include "SensorReading.hpp"
#include <nlohmann/json.hpp>

nlohmann::json SensorReading::serialize() {
  nlohmann::json j;
  j["value"] = value;
  j["min_value"] = min_value;
  j["max_value"] = max_value;
  j["sum"] = sum;
  j["times"] = times;

  return j;
}

void SensorReading::reset() {
  value = NAN;
  min_value = NAN;
  max_value = NAN;
  sum = 0;
  times = 0;
}
