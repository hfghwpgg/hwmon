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
         unsigned int divider, bool aggregateData = true);
  Sensor(std::shared_ptr<std::istream> dataStream, std::string name, SensorType type);
  virtual ~Sensor();

  void updateValue();
  SensorReading getReadings();
  virtual void resetReadings();
  nlohmann::json serialize();

  std::string getName();
  void setName(std::string name);
  SensorType getType();
  static SensorType deduceSensorType(std::string sensorName);


protected:
  virtual long double prepareValue();
  std::string readRawSensorString();
  bool aggregateData;

  std::shared_ptr<std::istream> dataStream;
  std::string name;
  const SensorType type;
  const unsigned int divider;
  SensorReading readings;
};
