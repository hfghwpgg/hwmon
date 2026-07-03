#include "Runner.hpp"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <map>

#include "Device.hpp"
#include "Devices/GeneralDevice.hpp"
#include "DeviceType.hpp"
#include "Devices/CpuDevice.hpp"
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

void Runner::setup() {
  const fs::path Path = "/sys/class/hwmon";
  // auto fail = std::make_unique<GeneralDevice>("asd", DeviceType::CPU, "fsa");
  // devices.push_back(std::move(fail));
  for (auto &entry : fs::directory_iterator(Path)) {
    // placeholder
    auto tmp =
        std::make_unique<GeneralDevice>(entry.path().filename(), DeviceType::UNKNOWN, entry.path());
    tmp->initialize();
    devices.push_back(std::move(tmp));
  }
};

// void Runner::setup() {
//   auto c = std::make_unique<CpuDevice>();
//   c->initialize();
//   devices.push_back(std::move(c));
// }

void Runner::run() {
  while (state.running.load(std::memory_order_relaxed)) {
    json serializedDevices = json::array();
    for (auto &device : devices) {
      device->read();
      serializedDevices.push_back(device->serialize());
    }

    // Publish the latest snapshot for clients to pull on request.
    state.snapshot.store(std::make_shared<const std::string>(serializedDevices.dump()),
                         std::memory_order_release);

    if (state.resetFlag.load(std::memory_order_relaxed)) {
      resetReadings();
      state.resetFlag.store(false, std::memory_order_relaxed);
    }

    const unsigned int intervalMs = state.intervalMs.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds{intervalMs});
  }
}

void Runner::resetReadings() {
  for (auto &device : devices) {
    device->resetReadings();
  }
}
