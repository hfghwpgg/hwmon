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
  long double reading = stold(str);

  if (std::isnan(lastReading)) {
    lastReading = reading;
    return NAN;
  }
  auto time = chrono::steady_clock::now();
  auto deltaTime = chrono::duration_cast<chrono::microseconds>(time - lastTime).count();
  // deliberately used microseconds, cuz interface is in micro joules
  auto ret = (reading - lastReading) / deltaTime;

  lastReading = reading;
  lastTime = time;
  return ret;
}
