#pragma once
#include "Sensor.hpp"
#include "SensorTypes.hpp"
#include <filesystem>
#include <format>
#include <memory>
#include <vector>

namespace fs = std::filesystem;

class Device {
public:
  Device(fs::path path, std::string name);
  Device(Device &&) noexcept = default;
  Device &operator=(Device &&) noexcept = default;
  Device(const Device &) noexcept = default;

  ~Device();
  SensorType DeduceSensorType(std::string parsedPart1);
  void createSensors(std::unordered_map<std::string, std::vector<std::string>> available_sensors);
  void Initialize();
  void Read();

  // TEMPORARY, TO BE REMOVED
  void Display();

private:
  fs::path path;
  std::string name;
  std::vector<std::unique_ptr<Sensor>> Sensors;
};