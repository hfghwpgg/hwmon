#include "DeltaSensor.hpp"

#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>

namespace chrono = std::chrono;

DeltaSensor::DeltaSensor(std::unique_ptr<std::istream> file, std::string name, SensorType type,
                         unsigned int divider, bool aggregateData, bool isPrimary) :
    // delta sensor gets file desciptor as unique
    // as it doesnt need to be shared (for now)
    Sensor(std::move(file), name, type, divider, aggregateData, isPrimary) {
  lastReading.value = NAN;
  lastReading.time = chrono::steady_clock::now();
}

DeltaSensor::DeltaSensor(std::unique_ptr<std::istream> file, std::string name, SensorType type,
                         unsigned int divider, bool aggregateData) :
    DeltaSensor(std::move(file), name, type, divider, aggregateData, false) {}

DeltaSensor::DeltaSensor(std::unique_ptr<std::istream> file, std::string name, SensorType type,
                         unsigned int divider) :
    DeltaSensor(std::move(file), name, type, divider, true, false) {}

DeltaSensor::DeltaSensor(std::unique_ptr<std::istream> file, std::string name, SensorType type) :
    DeltaSensor(std::move(file), name, type, 1, true, false) {}

void DeltaSensor::resetReadings() {
  lastReading.value = NAN;
  lastReading.time = chrono::steady_clock::now();
  Sensor::resetReadings();
}

long double DeltaSensor::getData() {
  return Sensor::prepareValue();
}

long double DeltaSensor::prepareValue() {
  auto readData = getData();
  if (std::isnan(readData)) {
    return NAN;
  }

  if (std::isnan(lastReading.value)) {
    lastReading.value = readData;
    return NAN;
  }
  auto time = chrono::steady_clock::now();
  auto deltaTime = chrono::duration<double>(time - lastReading.time).count();
  long double ret = (readData - lastReading.value) / (deltaTime * divider);

  lastReading.value = readData;
  lastReading.time = time;
  return ret;
}
