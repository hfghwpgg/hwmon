#pragma once
#include "SensorReading.hpp"
#include "SensorTypes.hpp"
#include <filesystem>
#include <fstream>

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


protected:
  virtual float PrepareValue();
  std::string ReadRawSensorString();

  std::ifstream file;
  std::string name;
  const SensorType type;
  SensorReading readings;
};
