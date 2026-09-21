#include "AmdGpuDevice.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

#include "Device.hpp"
#include "GpuDetector.hpp"
#include "Libraries/RsmiLibrary.hpp"
#include "Sensor/Sensor.hpp"
#include "Sensor/SensorType.hpp"
#include "Sensor/TransformScale.hpp"
#include "SharedHwmonParser.hpp"
#include "helpers.hpp"

AmdGpuDevice::AmdGpuDevice(GpuCardInfo card, std::set<std::filesystem::path> &hwmonPaths) :
    AmdGpuDevice(std::move(card), hwmonPaths, true) {}

AmdGpuDevice::AmdGpuDevice(GpuCardInfo card, std::set<std::filesystem::path> &hwmonPaths,
                           bool allowRsmi) :
    Device(card.cardPath.filename().string(), DeviceType::GPU),
    card(std::move(card)),
    hwmonPaths(hwmonPaths),
    allowRsmi(allowRsmi) {}

void AmdGpuDevice::initialize() {
  if (!card.hwmonPath.empty())
    hwmonPaths.erase(card.hwmonPath);

  if (allowRsmi && setupRsmi()) {
    addHwmonSensors(true);
    return;
  }

  name = sysfsName();
  setupSysfs();
  addHwmonSensors(false);
}

void AmdGpuDevice::read() {
  readRsmi();
  for (const auto &sensor : sensors) {
    sensor->updateValue();
  }
}

void AmdGpuDevice::resetReadings() {
  for (const auto &sensor : sensors) {
    sensor->resetReadings();
  }
}

nlohmann::json AmdGpuDevice::serialize() {
  nlohmann::json j;
  j["name"] = name;
  j["type"] = DeviceType::GPU;
  for (const auto &sensor : sensors) {
    j["sensors"].push_back(sensor->serialize());
  }
  return j;
}

// Probes every metric once and only creates sensors for the ones the card
// answers
bool AmdGpuDevice::setupRsmi() {
  rsmi = RsmiLibrary::acquire();
  if (rsmi == nullptr)
    return false;

  if (!rsmi->findIndexByPciAddress(card.pciAddress, rsmiIndex)) {
    rsmi.reset();
    return false;
  }

  char deviceName[RSMI_DEVICE_NAME_BUFFER_SIZE];
  if (rsmi->rsmi_dev_market_name_get(rsmiIndex, deviceName, RSMI_DEVICE_NAME_BUFFER_SIZE) ==
      RSMI_STATUS_SUCCESS) {
    name = deviceName;
  } else if (deviceName[0] == '\0' && // returned name is empty
             rsmi->rsmi_dev_name_get(rsmiIndex, deviceName, RSMI_DEVICE_NAME_BUFFER_SIZE) ==
                 RSMI_STATUS_SUCCESS) {
    name = deviceName;
  } else {
    spdlog::warn("ROCm SMI: failed to get device name for {}", card.pciAddress);
    name = sysfsName();
  }

  int64_t temp = 0;
  if (rsmi->rsmi_dev_temp_metric_get(rsmiIndex, RSMI_TEMP_TYPE_EDGE, RSMI_TEMP_CURRENT, &temp) ==
      RSMI_STATUS_SUCCESS) {
    rsmiSensors.temp_edge =
        Sensor::addPushSensor<TransformScale>(sensors, {"GPU core", SensorType::TEMPERATURE});
  }

  if (rsmi->rsmi_dev_temp_metric_get(rsmiIndex, RSMI_TEMP_TYPE_JUNCTION, RSMI_TEMP_CURRENT,
                                     &temp) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.temp_junction =
        Sensor::addPushSensor<TransformScale>(sensors, {"GPU hotspot", SensorType::TEMPERATURE});
  }

  if (rsmi->rsmi_dev_temp_metric_get(rsmiIndex, RSMI_TEMP_TYPE_MEMORY, RSMI_TEMP_CURRENT, &temp) ==
      RSMI_STATUS_SUCCESS) {
    rsmiSensors.temp_vram =
        Sensor::addPushSensor<TransformScale>(sensors, {"GPU memory", SensorType::TEMPERATURE});
  }

  uint32_t utilization = 0;
  if (rsmi->rsmi_dev_busy_percent_get(rsmiIndex, &utilization) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.gpuBusy = Sensor::addPushSensor<TransformScale>(
        sensors, {"GPU utilization", SensorType::UTILIZATION});
  }
  if (rsmi->rsmi_dev_memory_busy_percent_get(rsmiIndex, &utilization) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.memBusy = Sensor::addPushSensor<TransformScale>(
        sensors, {"VRAM utilization", SensorType::UTILIZATION});
  }

  if (rsmi->getCurrentClockMhz(rsmiIndex, RSMI_CLK_TYPE_SYS) >= 0) {
    rsmiSensors.sclk =
        Sensor::addPushSensor<TransformScale>(sensors, {"GPU core", SensorType::FREQUENCY, 1});
  }
  if (rsmi->getCurrentClockMhz(rsmiIndex, RSMI_CLK_TYPE_MEM) >= 0) {
    rsmiSensors.mclk =
        Sensor::addPushSensor<TransformScale>(sensors, {"GPU memory", SensorType::FREQUENCY, 1});
  }

  uint64_t power = 0;
  if (rsmi->rsmi_dev_power_ave_get(rsmiIndex, 0, &power) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.power =
        Sensor::addPushSensor<TransformScale>(sensors, {"GPU power draw", SensorType::POWER});
  }

  uint64_t powerCap = 0;
  if (rsmi->rsmi_dev_power_cap_get(rsmiIndex, 0, &powerCap) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.powerCap =
        Sensor::addPushSensor<TransformScale>(sensors, {"GPU power cap", SensorType::POWER});
  }

  uint64_t vram = 0;
  if (rsmi->rsmi_dev_memory_total_get(rsmiIndex, RSMI_MEM_TYPE_VRAM, &vram) ==
      RSMI_STATUS_SUCCESS) {
    rsmiSensors.vramTotal =
        Sensor::addPushSensor<TransformScale>(sensors, {"GPU total memory", SensorType::MEMORY});
  }
  if (rsmi->rsmi_dev_memory_usage_get(rsmiIndex, RSMI_MEM_TYPE_VRAM, &vram) ==
      RSMI_STATUS_SUCCESS) {
    rsmiSensors.vramUsed =
        Sensor::addPushSensor<TransformScale>(sensors, {"GPU used memory", SensorType::MEMORY});
  }

  uint64_t tx = 0;
  uint64_t rx = 0;
  if (rsmi->rsmi_dev_pci_throughput_get(rsmiIndex, &tx, &rx, nullptr) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.pcieTx =
        Sensor::addPushSensor<TransformScale>(sensors, {"pcie_tx", SensorType::THROUGHPUT});
    rsmiSensors.pcieRx =
        Sensor::addPushSensor<TransformScale>(sensors, {"pcie_rx", SensorType::THROUGHPUT});
  }

  spdlog::info("using ROCm SMI for {} (index {})", name, rsmiIndex);
  return true;
}

void AmdGpuDevice::readRsmi() {
  if (rsmi == nullptr)
    return;

  if (rsmiSensors.temp_edge != nullptr) {
    int64_t temp = 0;
    if (rsmi->rsmi_dev_temp_metric_get(rsmiIndex, RSMI_TEMP_TYPE_EDGE, RSMI_TEMP_CURRENT, &temp) ==
        RSMI_STATUS_SUCCESS)
      rsmiSensors.temp_edge->setValue(static_cast<double>(temp)); // millidegrees
  }

  if (rsmiSensors.temp_junction != nullptr) {
    int64_t temp = 0;
    if (rsmi->rsmi_dev_temp_metric_get(rsmiIndex, RSMI_TEMP_TYPE_JUNCTION, RSMI_TEMP_CURRENT,
                                       &temp) == RSMI_STATUS_SUCCESS)
      rsmiSensors.temp_junction->setValue(static_cast<double>(temp)); // millidegrees
  }

  if (rsmiSensors.temp_vram != nullptr) {
    int64_t temp = 0;
    if (rsmi->rsmi_dev_temp_metric_get(rsmiIndex, RSMI_TEMP_TYPE_MEMORY, RSMI_TEMP_CURRENT,
                                       &temp) == RSMI_STATUS_SUCCESS)
      rsmiSensors.temp_vram->setValue(static_cast<double>(temp)); // millidegrees
  }

  if (rsmiSensors.gpuBusy != nullptr) {
    uint32_t utilization = 0;
    if (rsmi->rsmi_dev_busy_percent_get(rsmiIndex, &utilization) == RSMI_STATUS_SUCCESS)
      rsmiSensors.gpuBusy->setValue(utilization);
  }

  if (rsmiSensors.memBusy != nullptr) {
    uint32_t utilization = 0;
    if (rsmi->rsmi_dev_memory_busy_percent_get(rsmiIndex, &utilization) == RSMI_STATUS_SUCCESS)
      rsmiSensors.memBusy->setValue(utilization);
  }

  if (rsmiSensors.sclk != nullptr) {
    const long long clock = rsmi->getCurrentClockMhz(rsmiIndex, RSMI_CLK_TYPE_SYS);
    if (clock >= 0)
      rsmiSensors.sclk->setValue(clock);
  }

  if (rsmiSensors.mclk != nullptr) {
    const long long clock = rsmi->getCurrentClockMhz(rsmiIndex, RSMI_CLK_TYPE_MEM);
    if (clock >= 0)
      rsmiSensors.mclk->setValue(clock);
  }

  if (rsmiSensors.power != nullptr) {
    uint64_t power = 0;
    if (rsmi->rsmi_dev_power_ave_get(rsmiIndex, 0, &power) == RSMI_STATUS_SUCCESS)
      rsmiSensors.power->setValue(static_cast<double>(power)); // microwatts
  }

  if (rsmiSensors.powerCap != nullptr) {
    uint64_t powerCap = 0;
    if (rsmi->rsmi_dev_power_cap_get(rsmiIndex, 0, &powerCap) == RSMI_STATUS_SUCCESS)
      rsmiSensors.powerCap->setValue(static_cast<double>(powerCap)); // microwatts
  }

  if (rsmiSensors.vramTotal != nullptr) {
    uint64_t total = 0;
    if (rsmi->rsmi_dev_memory_total_get(rsmiIndex, RSMI_MEM_TYPE_VRAM, &total) ==
        RSMI_STATUS_SUCCESS)
      rsmiSensors.vramTotal->setValue(static_cast<double>(total));
  }

  if (rsmiSensors.vramUsed != nullptr) {
    uint64_t used = 0;
    if (rsmi->rsmi_dev_memory_usage_get(rsmiIndex, RSMI_MEM_TYPE_VRAM, &used) ==
        RSMI_STATUS_SUCCESS)
      rsmiSensors.vramUsed->setValue(static_cast<double>(used));
  }

  if (rsmiSensors.pcieTx != nullptr) {
    uint64_t tx = 0;
    uint64_t rx = 0;
    if (rsmi->rsmi_dev_pci_throughput_get(rsmiIndex, &tx, &rx, nullptr) == RSMI_STATUS_SUCCESS) {
      rsmiSensors.pcieTx->setValue(static_cast<double>(tx));
      rsmiSensors.pcieRx->setValue(static_cast<double>(rx));
    }
  }
}

// amdgpu exposes utilization and VRAM usage outside of hwmon, as plain
// integers under the DRM device directory
void AmdGpuDevice::setupSysfs() {
  addSysfsSensor(sensors, card.devicePath / "gpu_busy_percent", "GPU utilization",
                 SensorType::UTILIZATION, 1);
  addSysfsSensor(sensors, card.devicePath / "mem_busy_percent", "VRAM utilization",
                 SensorType::UTILIZATION, 1);
  addSysfsSensor(sensors, card.devicePath / "mem_info_vram_used", "GPU memory", SensorType::MEMORY,
                 1, true, true);
  addSysfsSensor(sensors, card.devicePath / "mem_info_vram_total", "GPU total memory",
                 SensorType::MEMORY, 1, false, false);

  if (sensors.empty() && card.hwmonPath.empty()) {
    spdlog::warn("{}: no readable metrics found", card.devicePath.string());
    return;
  }
  spdlog::info("using amdgpu sysfs for {}", name);
}

void AmdGpuDevice::addHwmonSensors(bool onlyUncoveredMetrics) {
  if (card.hwmonPath.empty())
    return;

  auto availableSensors = SharedHwmonParser::parseHwmonDirectory(card.hwmonPath);

  if (onlyUncoveredMetrics) {
    for (auto it = availableSensors.begin(); it != availableSensors.end();) {
      const SensorType type = Sensor::deduceSensorType(it->first);
      const bool keep = type == SensorType::FAN_SPEED || type == SensorType::VOLTAGE;
      it = keep ? std::next(it) : availableSensors.erase(it);
    }
  }

  SharedHwmonParser::createSensors(card.hwmonPath, availableSensors, sensors);
}

void AmdGpuDevice::addSysfsSensor(helpers::SensorVec &sensors, const std::filesystem::path &path,
                                  const std::string &sensorName, SensorType type,
                                  unsigned int divider, bool aggregateData, bool isPrimary) {
  if (!std::filesystem::exists(path))
    return;

  Sensor::makeFileSensor<TransformScale>(sensors, path,
                                         {sensorName, type, divider, aggregateData, isPrimary});
}

std::string AmdGpuDevice::sysfsName() const {
  return std::format("AMD GPU ({:04x}:{:04x})", card.vendorId, card.deviceId);
}
