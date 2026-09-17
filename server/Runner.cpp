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
#include "DeviceType.hpp"
#include "Devices/AmdGpuDevice.hpp"
#include "Devices/CpuDevice.hpp"
#include "Devices/GpuDetector.hpp"
#include "Devices/IntelGpuDevice.hpp"
#include "Devices/NetworkDevice.hpp"
#include "Devices/NvidiaGpuDevice.hpp"
#include "Devices/SysfsDevice.hpp"
#include "SharedState.hpp"
#include "helpers.hpp"

using nlohmann::json;

Runner::Runner(SharedState &state, helpers::fs::path hwmonPath, bool doSpecializedDevices,
               helpers::fs::path drmPath) :
    doSpecializedDevices(doSpecializedDevices),
    hwmonPath(hwmonPath),
    drmPath(drmPath),
    state(state) {
  devices.reserve(10);
};
#ifdef DEBUG
Runner::~Runner() {
  spdlog::trace("runner destroyed");
}
#endif

/* */

long Runner::getUnixTimestamp() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
void Runner::setup() {
  if (!helpers::fs::exists(hwmonPath) || access(hwmonPath.c_str(), R_OK) == -1) {
    spdlog::critical("no access to hwmon interface, aborting");
    throw std::runtime_error("no access to hwmon interface");
  }

  std::set<helpers::fs::path> hwmonPaths;
  for (const auto &entry : helpers::fs::directory_iterator(hwmonPath)) {
    hwmonPaths.insert(helpers::fs::canonical(entry.path()));
  }

  spdlog::trace("hwmon length: {}", hwmonPaths.size());

  if (doSpecializedDevices) {
    auto cpu = std::make_unique<CpuDevice>(hwmonPaths);
    cpu->initialize();
    devices.push_back(std::move(cpu));

    setupGpuDevices(hwmonPaths);
    setupNetworkDevice();
  }

  spdlog::trace("hwmon length: {}", hwmonPaths.size());
  // rest of hwmon devices
  for (const auto &entry : hwmonPaths) {
    auto dev = std::make_unique<SysfsDevice>(entry.filename(), DeviceType::UNKNOWN, entry);
    dev->initialize();
    devices.push_back(std::move(dev));
  }
}
/* */

// One device per physical card, created only for GPUs that are actually
// present. A card that fails to initialize is skipped rather than aborting
// startup, so a single broken GPU can't take the whole server down.
void Runner::setupGpuDevices(std::set<helpers::fs::path> &hwmonPaths) {
  bool intelPmuClaimed = false;

  for (const auto &card : GpuDetector::detect(drmPath)) {
    try {
      std::unique_ptr<Device> gpu;
      switch (card.vendorId) {
      case PCI_VENDOR_AMD:
        gpu = std::make_unique<AmdGpuDevice>(card, hwmonPaths);
        break;
      case PCI_VENDOR_NVIDIA:
        gpu = std::make_unique<NvidiaGpuDevice>(card, hwmonPaths);
        break;
      case PCI_VENDOR_INTEL:
        // the i915 perf PMU is process-wide, so only the first Intel card gets it
        gpu = std::make_unique<IntelGpuDevice>(card, hwmonPaths, !intelPmuClaimed);
        intelPmuClaimed = true;
        break;
      default:
        continue;
      }

      gpu->initialize();
      devices.push_back(std::move(gpu));
    } catch (const std::exception &e) {
      spdlog::error("failed to initialize gpu {}: {}", card.cardPath.string(), e.what());
    }
  }
}

void Runner::setupNetworkDevice() {
  const helpers::fs::path netDev = "/proc/net/dev";
  if (!helpers::fs::exists(netDev) || access(netDev.c_str(), R_OK) == -1) {
    spdlog::error("{} inaccessible; skipping networking", netDev.string());
    return;
  }
  auto net = std::make_unique<NetworkDevice>("Network speed");
  net->initialize();
  devices.push_back(std::move(net));
}

void Runner::run() {
  auto timestamp = getUnixTimestamp();
  while (state.running.load(std::memory_order_relaxed)) {
    if (state.resetFlag.load(std::memory_order_relaxed)) {
      spdlog::debug("reset initiated");
      resetReadings();
      timestamp = getUnixTimestamp();
      state.resetFlag.store(false, std::memory_order_relaxed);
    }

    json serializedDevices = json::array();
    serializedDevices.push_back({{"timestamp", timestamp}});
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
