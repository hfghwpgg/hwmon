#include "Sensor.hpp"
#include "SensorReading.hpp"
#include "SensorTypes.hpp"
#include <cmath>
#include <format>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>

using std::string;

Sensor::Sensor(fs::path path, string name, SensorType type) :
    file(path),
    name(name),
    type(type),
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


string Sensor::ReadRawSensorString() {
  file.clear();
  file.seekg(0);
  string str;
  std::getline(file, str);
  return str;
}

float Sensor::PrepareValue() {
  string str = ReadRawSensorString();
  return std::stof(str) / getDivider(type);
}

void Sensor::updateValue() {
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

SensorReading Sensor::getReadings() {
  return this->readings;
}