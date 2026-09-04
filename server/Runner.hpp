#pragma once
#include <filesystem>
#include <memory>
#include <set>
#include <vector>

#include "Device.hpp"
#include "SharedState.hpp"

struct SharedState;

class Runner {
public:
  explicit Runner(SharedState &state, std::filesystem::path hwmonPath, bool doSpecializedDevices,
                  std::filesystem::path drmPath = "/sys/class/drm");


#ifdef DEBUG
  ~Runner();
#endif

  void setup();
  void run();

private:
  const bool doSpecializedDevices;
  const std::filesystem::path hwmonPath;
  const std::filesystem::path drmPath;
  SharedState &state;
  std::vector<std::unique_ptr<Device>> devices;

  void resetReadings();
  void setupGpuDevices(std::set<std::filesystem::path> &hwmonPaths);
};
