#pragma once
#include "SensorReading.hpp"
#include "SensorType.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json_fwd.hpp>
#include <string>

class Sensor {
public:
  Sensor(std::filesystem::path path, std::string name, SensorType type);

  // #ifdef DEBUG
  //   // allow moving
  //   Sensor(Sensor &&) noexcept = default;

  //   // block copying
  //   Sensor &operator=(Sensor &&) noexcept = delete;
  //   Sensor(const Sensor &) = delete;
  //   Sensor &operator=(const Sensor &) = delete;
  // #endif
  virtual ~Sensor();


  void UpdateValue();
  SensorReading GetReadings();
  nlohmann::json Serialize();


protected:
  virtual float PrepareValue();
  std::string ReadRawSensorString();

  std::ifstream file;
  std::string name;
  const SensorType type;
  const int divider;
  SensorReading readings;
};
