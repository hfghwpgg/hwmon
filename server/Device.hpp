#pragma once
#include "Sensor.hpp"
#include "SensorTypes.hpp"
#include <filesystem>
#include <format>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <vector>

namespace fs = std::filesystem;

class Device {
public:
  Device(fs::path path, std::string name);
  Device(Device &&) noexcept = default;
  Device &operator=(Device &&) noexcept = default;
  Device(const Device &) noexcept = default;

  ~Device();
  void Read();
  nlohmann::json Serialize();

  // TEMPORARY, TO BE REMOVED
  void Display();

private:
  fs::path path;
  std::string name;
  std::vector<std::unique_ptr<Sensor>> Sensors;

  SensorType DeduceSensorType(std::string parsedPart1);
  void CreateSensors(std::unordered_map<std::string, std::vector<std::string>> availableSensors);
  void Initialize();
};
