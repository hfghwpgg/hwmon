#pragma once
#include "Device.hpp"

#include "Sensor/Source.hpp"
#include <filesystem>

class PushSource;

// Reads MemTotal and MemAvailable from /proc/meminfo (values in bytes).
class RamDevice : public Device {
public:
  explicit RamDevice(std::filesystem::path meminfoPath = "/proc/meminfo");

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  const std::filesystem::path meminfoPath;
  PushSource *totalSrc = nullptr;
  PushSource *availableSrc = nullptr;
};
