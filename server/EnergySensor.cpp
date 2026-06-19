#include "EnergySensor.hpp"
#include <chrono>
#include <cmath>


enum class SensorType;
namespace fs = std::filesystem;
namespace chrono = std::chrono;
using std::string;

EnergySensor::EnergySensor(fs::path path, string name, SensorType type) :
    Sensor(path, name, type) {
  lastReading = NAN;
  lastTime = chrono::steady_clock::now();
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

  if (std::isnan(lastReading)) {
    lastReading = readData;
    return NAN;
  }
  auto time = chrono::steady_clock::now();
  auto deltaTime = chrono::duration_cast<chrono::microseconds>(time - lastTime).count();
  // deliberately used microseconds, cuz interface is in micro joules
  auto ret = (readData - lastReading) / deltaTime;

  lastReading = readData;
  lastTime = time;
  return ret;
}
