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
         unsigned int divider, bool aggregateData = true, bool isPrimary = false);
  Sensor(std::shared_ptr<std::istream> dataStream, std::string name, SensorType type);
  virtual ~Sensor();

  void updateValue();
  SensorReading getReadings();
  virtual void resetReadings();
  nlohmann::json serialize();
  void switchPrimary();

  std::string getName();
  void setName(std::string name);
  SensorType getType();
  static SensorType deduceSensorType(std::string sensorName);


protected:
  virtual long double prepareValue();
  std::string readRawSensorString();
  // whether to accumulate sum and times
  // variables
  bool aggregateData;
  // show as primary for a section
  // on client-side
  bool isPrimary;

  std::shared_ptr<std::istream> dataStream;
  std::string name;
  const SensorType type;
  const unsigned int divider;
  SensorReading readings;
};
