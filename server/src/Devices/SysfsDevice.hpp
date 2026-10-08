#pragma once
#include "Device.hpp"
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>

class SysfsDevice : public Device {
public:
  SysfsDevice(std::string name, DeviceType type, const std::filesystem::path &path);

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  std::filesystem::path path;
  void getName();
};
