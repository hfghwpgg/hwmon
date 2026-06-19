#pragma once
#include <nlohmann/json_fwd.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "DeviceType.hpp"
#include "Sensor.hpp"
#include "SensorType.hpp"

class Sensor;
enum class DeviceType;


namespace fs = std::filesystem;

class Device {
public:
  Device(fs::path path, std::string name, DeviceType type);

#ifdef DEBUG
  Device(Device &&) noexcept = default;
  Device &operator=(Device &&) noexcept = default;

  Device(const Device &) noexcept = delete;

  ~Device();
#endif

  void read();
  nlohmann::json serialize();

private:
  fs::path path;
  std::string name;
  DeviceType type;
  std::vector<std::unique_ptr<Sensor>> sensors;

  SensorType deduceSensorType(std::string parsedPart1);
  void createSensors(std::unordered_map<std::string, std::vector<std::string>> availableSensors);
  void initialize();
};
