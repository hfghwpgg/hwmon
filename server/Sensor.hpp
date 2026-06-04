#pragma once
#include "SensorReading.hpp"
#include "SensorTypes.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json_fwd.hpp>

namespace fs = std::filesystem;


class Sensor {
public:
  Sensor(fs::path path, std::string name, SensorType type);

  // allow moving
  Sensor(Sensor &&) noexcept = default;

  // block copying
  Sensor &operator=(Sensor &&) noexcept = delete;
  Sensor(const Sensor &) = delete;
  Sensor &operator=(const Sensor &) = delete;

  virtual ~Sensor();


  void updateValue();
  SensorReading getReadings();
  nlohmann::json Serialize();


protected:
  virtual float prepareValue();
  std::string readRawSensorString();

  std::ifstream file;
  std::string name;
  const SensorType type;
  const int divider;
  SensorReading readings;
};
