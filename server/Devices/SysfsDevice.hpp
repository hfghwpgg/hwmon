#pragma once
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "../Device.hpp"
#include "../hwmon.hpp"

class SysfsDevice : public Device {
public:
  SysfsDevice(std::string name, DeviceType type, hwmon::fs::path path);

#ifdef DEBUG
  ~SysfsDevice();
#endif

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  hwmon::fs::path path;
  void getName();
};
