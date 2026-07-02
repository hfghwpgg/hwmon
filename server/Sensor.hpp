#pragma once
#include <istream>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <filesystem>
#include <string>

#include "SensorReading.hpp"
#include "SensorType.hpp"

enum class SensorType;

class Sensor {
public:
  Sensor(std::shared_ptr<std::istream> file, std::string name, SensorType type,
         unsigned int divider);
  Sensor(std::shared_ptr<std::istream> file, std::string name, SensorType type);
  virtual ~Sensor();

  void updateValue();
  SensorReading getReadings();
  nlohmann::json serialize();

protected:
  virtual float prepareValue();
  std::string readRawSensorString();

  std::shared_ptr<std::istream> file;
  std::string name;
  const SensorType type;
  const unsigned int divider;
  SensorReading readings;
};
