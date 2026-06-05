#include "Sensor.hpp"
#include "SensorReading.hpp"
#include "SensorTypes.hpp"
#include <cmath>
#include <format>
#include <iostream>
#include <nlohmann/json.hpp>
#include <print>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using std::println;
using std::string;

Sensor::Sensor(fs::path path, string name1, SensorType type) :
    file(path),
    name(name1),
    type(type),
    divider(GetDivider(type)),
    readings{0, NAN, NAN, 0, 0} {
  println("sensor init: {}\nits name: {}\n", path, name1);
  if (!file.is_open()) {
    throw std::runtime_error(std::format("unable to open file {}\n", path.string()));
  };
}

Sensor::~Sensor() {
  file.close();
  println("sensor destroyed: {}", this->name);
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
