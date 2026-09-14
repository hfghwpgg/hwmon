#include "EnergySensor.hpp"

#include <chrono>
#include <cmath>

namespace chrono = std::chrono;

EnergySensor::EnergySensor(std::unique_ptr<std::istream> file, std::string name, SensorType type,
                           unsigned int divider) :
    // energy sensor gets file desciptor as unique
    // as it doesnt need to be shared
    Sensor(std::move(file), name, type, divider) {
  lastReading.value = NAN;
  lastReading.time = chrono::steady_clock::now();
}

EnergySensor::EnergySensor(std::unique_ptr<std::istream> file, std::string name, SensorType type) :
    EnergySensor(std::move(file), name, type, 1) {}

void EnergySensor::resetReadings() {
  lastReading.value = NAN;
  lastReading.time = chrono::steady_clock::now();
  Sensor::resetReadings();
}

long double EnergySensor::prepareValue() {
  auto readData = Sensor::prepareValue();

  if (std::isnan(lastReading.value)) {
    lastReading.value = readData;
    return NAN;
  }
  auto time = chrono::steady_clock::now();
  auto deltaTime = chrono::duration_cast<chrono::microseconds>(time - lastReading.time).count();
  // deliberately used microseconds, cuz interface is in micro joules
  long double ret = (readData - lastReading.value) / deltaTime;

  lastReading.value = readData;
  lastReading.time = time;
  return ret;
}
