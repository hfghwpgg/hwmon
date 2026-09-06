#pragma once
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "../Device.hpp"

namespace fs = std::filesystem;

class SysfsDevice : public Device {
public:
  SysfsDevice(std::string name, DeviceType type, fs::path path);

#ifdef DEBUG
  ~SysfsDevice();
#endif

  void initialize() override;

private:
  fs::path path;

  void getName();
};
