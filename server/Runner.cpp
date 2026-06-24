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

// void Runner::setup() {
//   const fs::path Path = "/sys/class/hwmon";
//   for (auto &entry : fs::directory_iterator(Path)) {
//     // placeholder
//     this->devices.emplace_back(
//         std::make_unique<Device>(entry.path(), entry.path().filename(), DeviceType::UNKNOWN));
//   }
// };

void Runner::setup() {
  this->devices.emplace_back(std::make_unique<CpuDevice>());
}

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

    const unsigned int intervalMs = state.intervalMs.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds{intervalMs});
  }
}
