#include "EnergySensor.hpp"
#include <chrono>
#include <cmath>


enum class SensorType;
namespace chrono = std::chrono;
using std::string;

EnergySensor::EnergySensor(std::unique_ptr<std::istream> file, string name, SensorType type) :
    // energy sensor gets file desciptor as unique
    // as it doesnt need to be shared
    Sensor(std::move(file), name, type) {
  lastReading.value = NAN;
  lastReading.time = chrono::steady_clock::now();
}

float EnergySensor::prepareValue() {
  string str = readRawSensorString();
  long double readData;
  try {
    readData = std::stof(str);
  } catch (const std::invalid_argument &) { // makes compilator happy
    return NAN;
  } catch (const std::out_of_range &) {
    return NAN;
  }

  if (std::isnan(lastReading.value)) {
    lastReading.value = readData;
    return NAN;
  }
  auto time = chrono::steady_clock::now();
  auto deltaTime = chrono::duration_cast<chrono::microseconds>(time - lastReading.time).count();
  // deliberately used microseconds, cuz interface is in micro joules
  auto ret = (readData - lastReading.value) / deltaTime;

  lastReading.value = readData;
  lastReading.time = time;
  return ret;
}
