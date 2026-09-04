#include "ValueSensor.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Sensor.hpp"
#include "SensorType.hpp"

ValueSensor::ValueSensor(std::string name, SensorType type) :
    Sensor(nullptr, name, type, 1),
    pendingValue(NAN) {}

void ValueSensor::setValue(long double value) {
  pendingValue = value;
}

void ValueSensor::invalidate() {
  pendingValue = NAN;
}

void ValueSensor::resetReadings() {
  pendingValue = NAN;
  Sensor::resetReadings();
}

// the value is consumed once, so a backend that stops reporting
// doesn't keep the last reading alive forever
long double ValueSensor::prepareValue() {
  const long double value = pendingValue;
  pendingValue = NAN;
  return value;
}

ValueSensor *addValueSensor(std::vector<std::unique_ptr<Sensor>> &sensors, std::string name,
                            SensorType type) {
  auto sensor = std::make_unique<ValueSensor>(std::move(name), type);
  ValueSensor *borrowed = sensor.get();
  sensors.emplace_back(std::move(sensor));
  return borrowed;
}
