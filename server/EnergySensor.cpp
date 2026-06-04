#include "EnergySensor.hpp"
#include "SensorTypes.hpp"
#include <chrono>
#include <cmath>
#include <iostream>

using std::string;

EnergySensor::EnergySensor(fs::path path, string name, SensorType type) :
    Sensor(path, name, type) {
  lastReading = NAN;
  lastTime = std::chrono::steady_clock::now();
  std::cout << "this is an energy sensor" << std::endl;
}

float EnergySensor::PrepareValue() {
  string str = ReadRawSensorString();
  long double reading = stold(str);

  if (std::isnan(lastReading)) {
    lastReading = reading;
    return NAN;
  }
  auto time = std::chrono::steady_clock::now();
  auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(time - lastTime).count();
  // deliberately used microseconds, cuz interface is in micro joules
  auto ret = (reading - lastReading) / deltaTime;

  lastReading = reading;
  lastTime = time;
  return ret;
}
