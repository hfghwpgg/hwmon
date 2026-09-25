#pragma once
#include "Device.hpp"

#include <string>

class NetworkDevice : public Device {
public:
  NetworkDevice(std::string name, std::filesystem::path statsPath);

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  const std::filesystem::path statsPath;
};
