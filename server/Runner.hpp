#pragma once
#include <filesystem>
#include <memory>
#include <set>
#include <vector>

#include "Device.hpp"
#include "SharedState.hpp"
#include "hwmon.hpp"

struct SharedState;

class Runner {
public:
  explicit Runner(SharedState &state, hwmon::fs::path hwmonPath, bool doSpecializedDevices,
                  hwmon::fs::path drmPath = "/sys/class/drm");


#ifdef DEBUG
  ~Runner();
#endif

  void setup();
  void run();

private:
  const bool doSpecializedDevices;
  const hwmon::fs::path hwmonPath;
  const hwmon::fs::path drmPath;
  SharedState &state;
  std::vector<std::unique_ptr<Device>> devices;

  void resetReadings();
  void setupGpuDevices(std::set<hwmon::fs::path> &hwmonPaths);
  static long getUnixTimestamp();
};
