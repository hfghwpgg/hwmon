#include "EnergySensor.hpp"
#include "Sensor.hpp"
#include <chrono>
#include <cmath>

namespace chrono = std::chrono;

EnergySensor::EnergySensor(std::unique_ptr<std::istream> file, std::string name, SensorType type) :
    // energy sensor gets file desciptor as unique
    // as it doesnt need to be shared
    Sensor(std::move(file), name, type) {
  lastReading.value = NAN;
  lastReading.time = chrono::steady_clock::now();
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
