#include "Runner.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
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

Runner::Runner(SharedState &state) :
    state(state) {
  devices.reserve(10);
  setup();
};
#ifdef DEBUG
Runner::~Runner() {
  spdlog::debug("runner destroyed");
}
#endif

// placeholder
// void Runner::setup() {
//   const fs::path Path = "/sys/class/hwmon";
//   for (const auto &entry : fs::directory_iterator(Path)) {
//     auto tmp =
//         std::make_unique<GeneralDevice>(entry.path().filename(), DeviceType::UNKNOWN,
//         entry.path());
//     tmp->initialize();
//     devices.push_back(std::move(tmp));
//   }
// };

void Runner::setup() {
  const fs::path Path = "/sys/class/hwmon";
  if (!fs::exists(Path) || access(Path.c_str(), R_OK) == -1) {
    spdlog::critical("no access to hwmon interface, aborting");
    throw std::runtime_error("no access to hwmon interface");
  }

  std::vector<fs::path> hwmonPaths;
  for (const auto &entry : fs::directory_iterator(Path)) {
    hwmonPaths.push_back(fs::canonical(entry.path()));
  }

  auto cpu = std::make_unique<CpuDevice>(hwmonPaths);
  cpu->initialize();
  devices.push_back(std::move(cpu));
}

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
