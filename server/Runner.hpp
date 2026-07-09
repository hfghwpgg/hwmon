#pragma once
#include <filesystem>
#include <memory>
#include <vector>

#include "Device.hpp"
#include "SharedState.hpp"

struct SharedState;

class Runner {
public:
  explicit Runner(SharedState &state, std::filesystem::path hwmonPath, bool doSpecializedDevices);


#ifdef DEBUG
  ~Runner();
#endif

  void setup();
  void run();

private:
  const bool doSpecializedDevices;
  const std::filesystem::path hwmonPath;
  SharedState &state;
  std::vector<std::unique_ptr<Device>> devices;

  void resetReadings();
};
