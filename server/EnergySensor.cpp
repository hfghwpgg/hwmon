#include "EnergySensor.hpp"
#include "SensorTypes.hpp"
#include "roundFloat.hpp"
#include <chrono>
#include <cmath>
#include <iostream>

EnergySensor::EnergySensor(fs::path path, string name, SensorType type) :
    Sensor(path, name, type) {
  lastReading = NAN;
  lastTime = std::chrono::steady_clock::now();
  std::cout << "this is an energy sensor" << std::endl;
}

float EnergySensor::readAndPrepareValue() {
  std::cout << "IM UISED" << std::endl;

  file.seekg(0);
  string str;
  getline(file, str);

  long double reading = stold(str);

  if (std::isnan(lastReading)) {
    lastReading = reading;
    return NAN;
  }
  auto time = std::chrono::steady_clock::now();
  auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(time - lastTime).count();

  return roundFloat((reading - lastReading) / deltaTime, 2);
}