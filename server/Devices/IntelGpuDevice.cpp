#include "IntelGpuDevice.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <set>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>

#include "../Device.hpp"
#include "../SensorType.hpp"
#include "../ValueSensor.hpp"
#include "GpuDetector.hpp"
#include "SharedHwmonParser.hpp"

// Redefining C++ keywords fortunately has a warning in clang, however it's
// unavoidable here since the C library uses "class" as a struct member and
// keywords are not allowed to be used as identifiers in C++.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif

#define class class_
extern "C" {
#include "../Libraries/intel_gpu_top/intel_gpu_top.h"
}
#undef class

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// the engines struct grows by one trailing engine per discovered engine, so
// indexing has to go through the last member, like upstream intel_gpu_top does
#define engine_ptr(engines, n) (&(engines)->engine + (n))

namespace fs = std::filesystem;

namespace {

// the perf PMU name for the i915 device; discover_engines keeps the pointer,
// so it has to outlive the engines struct
constexpr const char *PMU_DEVICE = "i915";

} // namespace

IntelGpuDevice::IntelGpuDevice(GpuCardInfo card, std::set<fs::path> &hwmonPaths, bool allowPmu) :
    Device(card.cardPath.filename().string(), DeviceType::GPU),
    card(std::move(card)),
    hwmonPaths(hwmonPaths),
    allowPmu(allowPmu) {}

IntelGpuDevice::~IntelGpuDevice() {
  if (pmuEngines != nullptr)
    free_engines(pmuEngines);
}

void IntelGpuDevice::initialize() {
  if (!card.hwmonPath.empty())
    hwmonPaths.erase(card.hwmonPath);

  name = lookupName();

  const bool havePmu = allowPmu && card.driver == "i915" && setupPmu();

  // i915 and xe both expose power, energy and voltage through hwmon
  addHwmonSensors();

  if (!havePmu && sensors.empty())
    spdlog::warn("{}: no readable metrics found", card.devicePath.string());
}

void IntelGpuDevice::read() {
  readPmu();
  Device::read();
}

// The i915 PMU is read through perf counters, which needs either root or a
// permissive perf_event_paranoid; failing to open them is not fatal.
bool IntelGpuDevice::setupPmu() {
  pmuEngines = discover_engines(PMU_DEVICE);
  if (pmuEngines == nullptr) {
    spdlog::debug("intel gpu: no PMU engines found for {}", PMU_DEVICE);
    return false;
  }

  if (pmu_init(pmuEngines) != 0) {
    spdlog::warn("intel gpu: failed to initialize PMU, engine utilization unavailable");
    free_engines(pmuEngines);
    pmuEngines = nullptr;
    return false;
  }

  // establish the first sample so the next read has a delta to work with
  pmu_sample(pmuEngines);

  gpuUtil = addValueSensor(sensors, "gpu_util", SensorType::UTILIZATION);

  engineUtil.reserve(pmuEngines->num_engines);
  for (unsigned int i = 0; i < pmuEngines->num_engines; ++i) {
    const struct engine *engine = engine_ptr(pmuEngines, i);
    const char *engineName = engine->display_name != nullptr ? engine->display_name : engine->name;
    engineUtil.push_back(
        addValueSensor(sensors, std::format("{}_util", engineName), SensorType::UTILIZATION));
  }

  if (pmuEngines->freq_act.present)
    frequency = addValueSensor(sensors, "gpu_clock", SensorType::FREQUENCY);
  if (pmuEngines->num_rapl > 0)
    power = addValueSensor(sensors, "gpu_power", SensorType::POWER);

  spdlog::info("using i915 PMU for {} ({} engines)", name, pmuEngines->num_engines);
  return true;
}

void IntelGpuDevice::readPmu() {
  if (pmuEngines == nullptr)
    return;

  pmu_sample(pmuEngines);
  const double interval =
      static_cast<double>(pmuEngines->ts.cur - pmuEngines->ts.prev) / 1e9; // seconds
  if (interval <= 0)
    return;

  double maxUtil = 0;
  for (unsigned int i = 0; i < pmuEngines->num_engines; ++i) {
    struct engine *engine = engine_ptr(pmuEngines, i);
    const double util = pmu_calc(&engine->busy.val, 1e9, interval, 100);
    engineUtil[i]->setValue(util);
    maxUtil = std::max(maxUtil, util);
  }
  gpuUtil->setValue(maxUtil);

  if (frequency != nullptr)
    frequency->setValue(pmu_calc(&pmuEngines->freq_act.val, 1, interval, 1)); // MHz
  if (power != nullptr)
    power->setValue(pmu_calc(&pmuEngines->r_gpu.val, 1, interval, pmuEngines->r_gpu.scale)); // W
}

void IntelGpuDevice::addHwmonSensors() {
  if (card.hwmonPath.empty())
    return;

  const auto availableSensors = SharedHwmonParser::parseHwmonDirectory(card.hwmonPath);
  SharedHwmonParser::createSensors(card.hwmonPath, availableSensors, sensors);
}

// intel_gpu_top ships a PCI id to marketing name table
std::string IntelGpuDevice::lookupName() const {
  char *deviceId = get_intel_device_id(card.cardPath.c_str());
  if (deviceId == nullptr)
    return std::format("Intel GPU ({:04x}:{:04x})", card.vendorId, card.deviceId);

  char *deviceName = get_intel_device_name(deviceId);
  free(deviceId);
  if (deviceName == nullptr)
    return std::format("Intel GPU ({:04x}:{:04x})", card.vendorId, card.deviceId);

  std::string result{deviceName};
  free(deviceName);
  return result;
}
