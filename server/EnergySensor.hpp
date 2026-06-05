#pragma once
#include "Sensor.hpp"
#include "SensorTypes.hpp"
#include <chrono>

class EnergySensor : public Sensor {
public:
  EnergySensor(std::filesystem::path path, std::string name, SensorType type);

private:
  float PrepareValue() override;

private:
  long double lastReading;
  std::chrono::steady_clock::time_point lastTime;
};
