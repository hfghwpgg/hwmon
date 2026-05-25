#pragma once
#include "SensorReading.hpp"
#include "SensorTypes.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using std::ifstream;
using std::string;

class Sensor {
public:
  Sensor(fs::path path, string name, SensorType type);

  // allow moving
  Sensor(Sensor &&) noexcept = default;
  Sensor &operator=(Sensor &&) noexcept = default;

  // block copying
  Sensor(const Sensor &) = delete;
  Sensor &operator=(const Sensor &) = delete;

  ~Sensor();
  void updateValue();
  SensorReading getReadings();


protected:
  virtual float readAndPrepareValue();

  ifstream file;
  string name;
  const SensorType type;
  SensorReading readings;
};
