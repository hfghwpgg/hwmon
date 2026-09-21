#pragma once
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "../Device.hpp"
#include "../helpers.hpp"

class SysfsDevice : public Device {
public:
  SysfsDevice(std::string name, DeviceType type, std::filesystem::path path);

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  std::filesystem::path path;
  void getName();
};
