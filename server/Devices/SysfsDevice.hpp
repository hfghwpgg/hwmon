#pragma once
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "../Device.hpp"
#include "../Sensor.hpp"

namespace fs = std::filesystem;

class SysfsDevice : public Device {
public:
  SysfsDevice(std::string name, DeviceType type, fs::path path);

#ifdef DEBUG
  ~SysfsDevice();
#endif

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  fs::path path;
  std::vector<std::unique_ptr<Sensor>> sensors;
  void getName();
};
