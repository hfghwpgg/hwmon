#include "Runner.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <limits>
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
#include "Devices/CpuDevice.hpp"
#include "Devices/GPU/AmdGpuDevice.hpp"
#include "Devices/GPU/GpuDetector.hpp"
#include "Devices/GPU/IntelGpuDevice.hpp"
#include "Devices/GPU/NvidiaGpuDevice.hpp"
#include "Devices/NetworkDevice.hpp"
#include "Devices/SysfsDevice.hpp"
#include "SharedState.hpp"
#include "helpers.hpp"

using nlohmann::json;

Runner::Runner(SharedState &state, std::filesystem::path hwmonPath, bool doSpecializedDevices,
               std::filesystem::path drmPath) :
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

//  ===============
//  ==== SETUP ====
//  ===============
void Runner::setup() {
  if (!std::filesystem::exists(hwmonPath) || access(hwmonPath.c_str(), R_OK) == -1) {
    spdlog::critical("no access to hwmon interface, aborting");
    throw std::runtime_error("no access to hwmon interface");
  }

  std::set<std::filesystem::path> hwmonPaths;
  for (const auto &entry : std::filesystem::directory_iterator(hwmonPath)) {
    hwmonPaths.insert(std::filesystem::canonical(entry.path()));
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
void Runner::setupGpuDevices(std::set<std::filesystem::path> &hwmonPaths) {
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
  const std::filesystem::path sysfs_net = "/sys/class/net";
  if (!std::filesystem::exists(sysfs_net) || access(sysfs_net.c_str(), R_OK) == -1) {
    spdlog::error("{} inaccessible; skipping networking", sysfs_net.string());
    return;
  }

  for (const auto &dir : std::filesystem::directory_iterator(sysfs_net)) {
    const std::string devName = dir.path().stem().string();
    auto net = std::make_unique<NetworkDevice>(devName, dir.path() / "statistics");
    net->initialize();
    devices.push_back(std::move(net));
  }
}

//  ================
//  = END OF SETUP =
//  ================

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

    // Interruptible sleep: shutdown ends the wait instead of running out the
    // whole interval first.
    const unsigned int intervalMs = state.intervalMs.load(std::memory_order_relaxed);
    const int waitMs =
        static_cast<int>(std::min<unsigned int>(intervalMs, std::numeric_limits<int>::max()));
    if (state.waitForShutdown(waitMs)) {
      break;
    }
  }
}

void Runner::resetReadings() {
  for (const auto &device : devices) {
    device->resetReadings();
  }
}
