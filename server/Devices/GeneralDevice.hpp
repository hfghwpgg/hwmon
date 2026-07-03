#pragma once
#include <nlohmann/json_fwd.hpp>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Device.hpp"
#include "../DeviceType.hpp"

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

  void createSensors(std::unordered_map<std::string, std::vector<std::string>> availableSensors);
};
