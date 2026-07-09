#include "Runner.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <set>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "Device.hpp"
#include "Devices/CpuDevice.hpp"
#include "Devices/GeneralDevice.hpp"
#include "SharedState.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

Runner::Runner(SharedState &state, std::filesystem::path hwmonPath, bool doSpecializedDevices) :
    doSpecializedDevices(doSpecializedDevices),
    hwmonPath(hwmonPath),
    state(state) {
  devices.reserve(10);
};
#ifdef DEBUG
Runner::~Runner() {
  spdlog::debug("runner destroyed");
}
#endif

/* */
void Runner::setup() {
  if (!fs::exists(hwmonPath) || access(hwmonPath.c_str(), R_OK) == -1) {
    spdlog::critical("no access to hwmon interface, aborting");
    throw std::runtime_error("no access to hwmon interface");
  }

  std::set<fs::path> hwmonPaths;
  for (const auto &entry : fs::directory_iterator(hwmonPath)) {
    hwmonPaths.insert(fs::canonical(entry.path()));
  }

  spdlog::debug("hwmon length: {}", hwmonPaths.size());
  if (doSpecializedDevices) {
    auto cpu = std::make_unique<CpuDevice>(hwmonPaths);
    cpu->initialize();
    devices.push_back(std::move(cpu));
  }

  spdlog::debug("hwmon length: {}", hwmonPaths.size());

  // rest of hwmon devices
  for (const auto &entry : hwmonPaths) {
    auto dev = std::make_unique<GeneralDevice>(entry.filename(), DeviceType::UNKNOWN, entry);
    dev->initialize();
    devices.push_back(std::move(dev));
  }
}
/* */

void Runner::run() {
  while (state.running.load(std::memory_order_relaxed)) {
    if (state.resetFlag.load(std::memory_order_relaxed)) {
      resetReadings();
      state.resetFlag.store(false, std::memory_order_relaxed);
    }

    json serializedDevices = json::array();
    for (const auto &device : devices) {
      device->read();
      serializedDevices.push_back(device->serialize());
    }

    // Publish the latest snapshot for clients to pull on request.
    state.snapshot.store(std::make_shared<const std::string>(serializedDevices.dump()),
                         std::memory_order_release);

    const unsigned int intervalMs = state.intervalMs.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds{intervalMs});
  }
}

void Runner::resetReadings() {
  for (const auto &device : devices) {
    device->resetReadings();
  }
}
