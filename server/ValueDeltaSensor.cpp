#include "ValueDeltaSensor.hpp"

#include <chrono>
#include <cmath>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

#include "DeltaSensor.hpp"
#include "Sensor.hpp"
#include "SensorType.hpp"
#include "helpers.hpp"

ValueDeltaSensor::ValueDeltaSensor(std::string name, SensorType type, bool aggregateData,
                                   bool isPrimary) :
    DeltaSensor(nullptr, name, type, 1, aggregateData, isPrimary),
    pendingValue(NAN) {}

void ValueDeltaSensor::setValue(long double value) {
  pendingValue = value;
}

void ValueDeltaSensor::invalidate() {
  lastReading.value = NAN;
  pendingValue = NAN;
}

void ValueDeltaSensor::resetReadings() {
  pendingValue = NAN;
  DeltaSensor::resetReadings();
}

// the value is consumed once, so a backend that stops reporting
// doesn't keep the last reading alive forever
long double ValueDeltaSensor::getData() {
  const long double value = pendingValue;
  pendingValue = NAN;
  return value;
}

ValueDeltaSensor *addValueDeltaSensor(helpers::SensorVec &sensors, std::string name,
                                      SensorType type, bool aggregateData, bool isPrimary) {
  auto sensor = std::make_unique<ValueDeltaSensor>(std::move(name), type, aggregateData, isPrimary);
  ValueDeltaSensor *borrowed = sensor.get();
  sensors.emplace_back(std::move(sensor));
  return borrowed;
}
