#pragma once
#include <nlohmann/json_fwd.hpp>
#include <string>

#include "DeviceType.hpp"
#include "helpers.hpp"

class Device {
public:
  virtual ~Device();

  virtual void initialize() = 0;
  virtual void read() = 0;
  virtual nlohmann::json serialize() = 0;
  virtual void resetReadings() = 0;

protected:
  Device(std::string name, DeviceType type);

  std::string name;
  DeviceType type;
  helpers::SensorVec sensors;
};
