#include "Sensor.hpp"
#include "SensorReading.hpp"
#include "SensorTypes.hpp"
#include <cmath>
#include <format>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <ostream>
#include <stdexcept>
#include <string>

using std::string;

Sensor::Sensor(fs::path path, string name, SensorType type) :
    file(path),
    name(name),
    type(type),
    divider(getDivider(type)),
    readings{0, NAN, NAN, 0, 0} {
  std::cout << "sensor init: " << path << std::endl;
  if (!file.is_open()) {
    throw std::runtime_error(std::format("unable to open file {}", path.string()));
  };
}

Sensor::~Sensor() {
  file.close();
  std::cout << "sensor destroyed: " << this->name << std::endl;
}


string Sensor::readRawSensorString() {
  file.clear();
  file.seekg(0);
  string str;
  std::getline(file, str);
  return str;
}

float Sensor::prepareValue() {
  string str = readRawSensorString();
  return std::stof(str) / divider;
}

void Sensor::updateValue() {
  float value = prepareValue();
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

SensorReading Sensor::getReadings() {
  return this->readings;
}

nlohmann::json Sensor::Serialize() {
  nlohmann::json j;
  j["name"] = this->name;
  j["type"] = this->type;
  j["readings"] = this->readings.serialize();
  return j;
}
