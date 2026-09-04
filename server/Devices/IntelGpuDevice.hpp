#pragma once
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "../Device.hpp"
#include "../ValueSensor.hpp"
#include "GpuDetector.hpp"

struct engines;

class IntelGpuDevice : public Device {
public:
  // hwmonPaths is the set Runner hands out; the card's own hwmon directory is
  // removed from it so it doesn't show up again as a GeneralDevice.
  // allowPmu must be set for at most one card: the i915 perf PMU is a
  // process-wide resource and the upstream helper only handles one device.
  IntelGpuDevice(GpuCardInfo card, std::set<std::filesystem::path> &hwmonPaths, bool allowPmu);
  ~IntelGpuDevice();

  void initialize() override;
  void read() override;

private:
  const GpuCardInfo card;
  std::set<std::filesystem::path> &hwmonPaths;
  const bool allowPmu;

  struct engines *pmuEngines = nullptr;
  ValueSensor *gpuUtil = nullptr;
  ValueSensor *frequency = nullptr;
  ValueSensor *power = nullptr;
  std::vector<ValueSensor *> engineUtil; // indexed like engines->engine[]

  bool setupPmu();
  void readPmu();
  void addHwmonSensors();
  std::string lookupName() const;
};
