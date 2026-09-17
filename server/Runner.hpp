#pragma once
#include <filesystem>
#include <memory>
#include <set>
#include <vector>

#include "Device.hpp"
#include "SharedState.hpp"
#include "helpers.hpp"

struct SharedState;

class Runner {
public:
  explicit Runner(SharedState &state, helpers::fs::path hwmonPath, bool doSpecializedDevices,
                  helpers::fs::path drmPath = "/sys/class/drm");


#ifdef DEBUG
  ~Runner();
#endif

  void setup();
  void run();

private:
  const bool doSpecializedDevices;
  const helpers::fs::path hwmonPath;
  const helpers::fs::path drmPath;
  SharedState &state;
  std::vector<std::unique_ptr<Device>> devices;

  void resetReadings();
  void setupGpuDevices(std::set<helpers::fs::path> &hwmonPaths);
  void setupNetworkDevice();
  static long getUnixTimestamp();
};
