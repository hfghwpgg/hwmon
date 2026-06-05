#include "EnergySensor.hpp"
#include "SensorTypes.hpp"
#include <chrono>
#include <cmath>
#include <print>

namespace fs = std::filesystem;
namespace chrono = std::chrono;
using std::println;
using std::string;

EnergySensor::EnergySensor(fs::path path, string name, SensorType type) :
    Sensor(path, name, type) {
  lastReading = NAN;
  lastTime = std::chrono::steady_clock::now();
  println("this is an energy sensor");
}

float EnergySensor::PrepareValue() {
  string str = ReadRawSensorString();
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
