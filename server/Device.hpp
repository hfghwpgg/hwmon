#pragma once
#include "Sensor.hpp"
#include "SensorTypes.hpp"
#include <filesystem>
#include <format>
#include <memory>
#include <vector>

namespace fs = std::filesystem;
using std::format;
using std::string;
using std::unordered_map;
using std::vector;

class Device {
public:
  Device(fs::path path, string name);
  Device(Device &&) noexcept = default;
  Device &operator=(Device &&) noexcept = default;
  Device(const Device &) noexcept = default;

  ~Device();
  SensorType DeduceSensorType(string parsedPart1);
  void createSensors(unordered_map<string, vector<string>> available_sensors);
  void Initialize();
  void Read();

  // TEMPORARY, TO BE REMOVED
  void Display();

private:
  fs::path path;
  string name;
  vector<std::unique_ptr<Sensor>> Sensors;
};