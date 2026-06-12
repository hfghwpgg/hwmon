#include "Sensor.hpp"

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <fmt/base.h>
#include <cmath>
#include <format>
#include <stdexcept>

#include "SensorReading.hpp"
#include "SensorType.hpp"


namespace fs = std::filesystem;
using std::string;

Sensor::Sensor(fs::path path, string name, SensorType type) :
    file(path),
    name(name),
    type(type),
    divider(GetDivider(type)),
    readings{0, NAN, NAN, 0, 0} {
#ifdef DEBUG
  spdlog::debug("sensor init: {}\nits name: {}\n", path.c_str(), name);
#endif
  if (!file.is_open()) {
    spdlog::critical("unable to open file {}\n", path.string());
    throw std::runtime_error(std::format("unable to open file {}\n", path.string()));
  };
}


Sensor::~Sensor() {
  file.close();
#ifdef DEBUG
  spdlog::debug("sensor destroyed: {}", this->name);
#endif
}


string Sensor::ReadRawSensorString() {
  file.clear();
  file.seekg(0);
  string str;
  std::getline(file, str);
  return str;
}

float Sensor::PrepareValue() {
  string str = ReadRawSensorString();
  return std::stof(str) / divider;
}

void Sensor::UpdateValue() {
  float value = PrepareValue();
  if (std::isnan(value))
    return;
  this->readings.value = value;
  this->readings.sum += value;
  this->readings.times++;

  if (this->readings.min_value > value || std::isnan(this->readings.min_value)) {
    this->readings.min_value = value;
  }
  if (this->readings.max_value < value || std::isnan(this->readings.max_value)) {
    this->readings.max_value = value;
  }
}

SensorReading Sensor::GetReadings() {
  return this->readings;
}

nlohmann::json Sensor::Serialize() {
  nlohmann::json j;
  j["name"] = this->name;
  j["type"] = this->type;
  j["readings"] = this->readings.Serialize();
  return j;
}
