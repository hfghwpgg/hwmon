#pragma once
#include <chrono>
#include <memory>
#include <string>

#include "Sensor.hpp"
#include "SensorType.hpp"

class EnergySensor : public Sensor {
public:
  EnergySensor(std::unique_ptr<std::istream> file, std::string name, SensorType type);

  void resetReadings() override;

private:
  long double prepareValue() override;

  struct {
    long double value;
    std::chrono::steady_clock::time_point time;
  } lastReading;
};
