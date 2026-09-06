#pragma once
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "Sensor.hpp"

enum class DeviceType { CPU, GPU, RAM, STORAGE, UNKNOWN };

class Device {
public:
  Device(std::string name, DeviceType type);

#ifdef DEBUG
  virtual ~Device();
#endif

  virtual void initialize() = 0;
  virtual void read();
  virtual nlohmann::json serialize();
  virtual void resetReadings();

protected:
  std::string name;
  DeviceType type;
  std::vector<std::unique_ptr<Sensor>> sensors;
};
