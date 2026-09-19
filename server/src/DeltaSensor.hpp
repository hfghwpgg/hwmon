#pragma once
#include <chrono>
#include <memory>
#include <string>

#include "Sensor.hpp"
#include "SensorType.hpp"

class DeltaSensor : public Sensor {
public:
  DeltaSensor(std::unique_ptr<std::istream> file, std::string name, SensorType type,
              unsigned int divider, bool aggregateData, bool isPrimary);
  DeltaSensor(std::unique_ptr<std::istream> file, std::string name, SensorType type,
              unsigned int divider, bool aggregateData);
  DeltaSensor(std::unique_ptr<std::istream> file, std::string name, SensorType type,
              unsigned int divider);
  DeltaSensor(std::unique_ptr<std::istream> file, std::string name, SensorType type);

  void resetReadings() override;

protected:
  // getData only exists, so it can be overriden
  // in DeltaValueSensor
  virtual long double getData();
  long double prepareValue() override;

  struct {
    long double value;
    std::chrono::steady_clock::time_point time;
  } lastReading;
};
