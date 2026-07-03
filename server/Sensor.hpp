#pragma once
#include <istream>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "SensorReading.hpp"
#include "SensorType.hpp"

enum class SensorType;

class Sensor {
public:
  Sensor(std::shared_ptr<std::istream> dataStream, std::string name, SensorType type,
         unsigned int divider);
  Sensor(std::shared_ptr<std::istream> dataStream, std::string name, SensorType type);
  virtual ~Sensor();

  void updateValue();
  SensorReading getReadings();
  void resetReadings();
  nlohmann::json serialize();

protected:
  virtual long double prepareValue();
  std::string readRawSensorString();

  std::shared_ptr<std::istream> dataStream;
  std::string name;
  const SensorType type;
  const unsigned int divider;
  SensorReading readings;
};
