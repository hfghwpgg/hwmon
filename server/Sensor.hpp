#pragma once
#include <nlohmann/json_fwd.hpp>
#include <filesystem>
#include <fstream>
#include <string>

#include "SensorReading.hpp"
#include "SensorType.hpp"

enum class SensorType;

class Sensor {
public:
  Sensor(std::filesystem::path path, std::string name, SensorType type);
  virtual ~Sensor();

  void updateValue();
  SensorReading getReadings();
  nlohmann::json serialize();

protected:
  virtual float prepareValue();
  std::string readRawSensorString();

  std::ifstream file;
  std::string name;
  const SensorType type;
  const unsigned int divider;
  SensorReading readings;
};
