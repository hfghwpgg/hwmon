#pragma once
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "DeviceType.hpp"

class Device {
public:
  Device(std::string name, DeviceType type);
  virtual ~Device();

  virtual void initialize() = 0;
  virtual void read() = 0;
  virtual nlohmann::json serialize() = 0;
  virtual void resetReadings() = 0;

protected:
  std::string name;
  DeviceType type;
};
