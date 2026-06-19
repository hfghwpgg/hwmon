#include "SensorReading.hpp"
#include <nlohmann/json.hpp>

nlohmann::json SensorReading::serialize() {
  nlohmann::json j;
  j["value"] = this->value;
  j["min_value"] = this->min_value;
  j["max_value"] = this->max_value;
  j["sum"] = this->sum;
  j["times"] = this->times;

  return j;
}
