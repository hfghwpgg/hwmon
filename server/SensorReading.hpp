#pragma once
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
struct SensorReading {
  float value;
  float min_value;
  float max_value;
  double sum;
  unsigned int times;

  nlohmann::json Serialize() {
    nlohmann::json j;
    j["value"] = this->value;
    j["min_value"] = this->min_value;
    j["max_value"] = this->max_value;
    j["sum"] = this->sum;
    j["times"] = this->times;

    return j;
  }
};
