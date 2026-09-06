#include "Sensor.hpp"

#include <cmath>
#include <memory>
#include <nlohmann/detail/exceptions.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "SensorReading.hpp"
#include "SensorType.hpp"

Sensor::Sensor(std::shared_ptr<std::istream> file, std::string name, SensorType type,
               unsigned int divider, bool aggregateData) :
    aggregateData(aggregateData),
    dataStream(file),
    name(name),
    type(type),
    divider(divider),
    readings{NAN, NAN, NAN, 0, 0} {
#ifdef DEBUG
  spdlog::debug("sensor init; its name: {}", name);
#endif
}

Sensor::Sensor(std::shared_ptr<std::istream> file, std::string name, SensorType type) :
    Sensor(file, name, type, getDivider(type)) {}

Sensor::~Sensor() {
  spdlog::debug("sensor destroyed: {}", name);
}

std::string Sensor::getName() {
  return name;
}
SensorType Sensor::getType() {
  return type;
}

std::string Sensor::readRawSensorString() {
  dataStream->clear();
  dataStream->seekg(0);
  std::string str;
  std::getline(*dataStream, str);
  return str;
}

// sysfs returns only integers by design
// but i use double to take advantage of
// NAN to communicate an error in a quiet
// way
double long Sensor::prepareValue() {
  std::string str = readRawSensorString();
  long double readData;
  try {
    readData = std::stold(str);
  } catch (const std::invalid_argument &) { // makes compilator happy
    return NAN;
  } catch (const std::out_of_range &) {
    return NAN;
  }
  return readData / divider;
}

void Sensor::updateValue() {
  double value = prepareValue();
  if (std::isnan(value)) {
    spdlog::debug("{}: value of recieved data is NaN", name);
    return;
  }

  readings.value = value;
  if (aggregateData) {
    readings.sum += value;
  }
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
