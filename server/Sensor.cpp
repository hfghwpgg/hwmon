#include "Sensor.hpp"

#include <memory>
#include <nlohmann/detail/exceptions.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <fmt/base.h>
#include <cmath>
#include <stdexcept>

#include "SensorReading.hpp"
#include "SensorType.hpp"

using std::string;

Sensor::Sensor(std::shared_ptr<std::istream> file, string name, SensorType type,
               unsigned int divider) :
    dataStream(file),
    name(name),
    type(type),
    divider(divider),
    readings{NAN, NAN, NAN, 0, 0} {
#ifdef DEBUG
  spdlog::debug("sensor init; its name: {}", name);
#endif
}

Sensor::Sensor(std::shared_ptr<std::istream> file, string name, SensorType type) :
    Sensor(file, name, type, getDivider(type)) {}

Sensor::~Sensor() {
  spdlog::debug("sensor destroyed: {}", name);
}

string Sensor::readRawSensorString() {
  dataStream->clear();
  dataStream->seekg(0);
  string str;
  std::getline(*dataStream, str);
  return str;
}

// sysfs returns only integers by design
// but i use float to take advantage of
// NAN to communicate an error in a quiet
// way
float Sensor::prepareValue() {
  string str = readRawSensorString();
  float readData;
  try {
    readData = std::stof(str);
  } catch (const std::invalid_argument &) { // makes compilator happy
    return NAN;
  } catch (const std::out_of_range &) {
    return NAN;
  }
  return readData / divider;
}

void Sensor::updateValue() {
  float value = prepareValue();
  if (std::isnan(value)) {
    spdlog::warn("{}: value of recieved data is NaN", name);
    return;
  }
  readings.value = value;
  readings.sum += value;
  readings.times++;

  if (readings.min_value > value || std::isnan(readings.min_value)) {
    readings.min_value = value;
  }
  if (readings.max_value < value || std::isnan(readings.max_value)) {
    readings.max_value = value;
  }
}

SensorReading Sensor::getReadings() {
  return readings;
}

void Sensor::resetReadings() {
  readings.reset();
}

nlohmann::json Sensor::serialize() {
  nlohmann::json j;
  j["name"] = name;
  j["type"] = type;
  j["readings"] = readings.serialize();
  return j;
}
