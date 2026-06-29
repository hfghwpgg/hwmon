#pragma once
#include <nlohmann/json_fwd.hpp>
#include <memory>
#include <string>
#include <vector>

#include "DeviceType.hpp"
#include "Sensor.hpp"

class Device {
public:
  Device(std::string name, DeviceType type);

#ifdef DEBUG
  virtual ~Device();
#endif

  virtual void initialize() = 0;
  virtual void read();
  virtual nlohmann::json serialize();

protected:
  std::string name;
  DeviceType type;
  std::vector<std::unique_ptr<Sensor>> sensors;
};
