#pragma once
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "../Device.hpp"
#include "../helpers.hpp"

class SysfsDevice : public Device {
public:
  SysfsDevice(std::string name, DeviceType type, helpers::fs::path path);

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  helpers::fs::path path;
  void getName();
};
