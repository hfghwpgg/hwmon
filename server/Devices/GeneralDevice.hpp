#pragma once
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "../Device.hpp"

namespace fs = std::filesystem;

class GeneralDevice : public Device {
public:
  GeneralDevice(std::string name, DeviceType type, fs::path path);

#ifdef DEBUG
  ~GeneralDevice();
#endif

  void initialize() override;

private:
  fs::path path;

  void getName();
};
