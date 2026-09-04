#include "AmdGpuDevice.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

#include "../Device.hpp"
#include "../Libraries/RsmiLibrary.hpp"
#include "../Sensor.hpp"
#include "../SensorType.hpp"
#include "../ValueSensor.hpp"
#include "../helpers.hpp"
#include "GpuDetector.hpp"
#include "SharedHwmonParser.hpp"

namespace fs = std::filesystem;

AmdGpuDevice::AmdGpuDevice(GpuCardInfo card, std::set<fs::path> &hwmonPaths) :
    AmdGpuDevice(std::move(card), hwmonPaths, true) {}

AmdGpuDevice::AmdGpuDevice(GpuCardInfo card, std::set<fs::path> &hwmonPaths, bool allowRsmi) :
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
  Device::read();
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
  if (rsmi->rsmi_dev_name_get(rsmiIndex, deviceName, RSMI_DEVICE_NAME_BUFFER_SIZE) ==
      RSMI_STATUS_SUCCESS) {
    name = deviceName;
  } else {
    spdlog::warn("ROCm SMI: failed to get device name for {}", card.pciAddress);
    name = sysfsName();
  }

  int64_t temp = 0;
  if (rsmi->rsmi_dev_temp_metric_get(rsmiIndex, RSMI_TEMP_TYPE_EDGE, RSMI_TEMP_CURRENT, &temp) ==
      RSMI_STATUS_SUCCESS) {
    rsmiSensors.temp = addValueSensor(sensors, "edge", SensorType::TEMPERATURE);
  }

  uint32_t utilization = 0;
  if (rsmi->rsmi_dev_busy_percent_get(rsmiIndex, &utilization) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.gpuBusy = addValueSensor(sensors, "gpu_busy", SensorType::UTILIZATION);
  }
  if (rsmi->rsmi_dev_memory_busy_percent_get(rsmiIndex, &utilization) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.memBusy = addValueSensor(sensors, "mem_busy", SensorType::UTILIZATION);
  }

  if (rsmi->getCurrentClockMhz(rsmiIndex, RSMI_CLK_TYPE_SYS) >= 0) {
    rsmiSensors.sclk = addValueSensor(sensors, "sclk", SensorType::FREQUENCY);
  }
  if (rsmi->getCurrentClockMhz(rsmiIndex, RSMI_CLK_TYPE_MEM) >= 0) {
    rsmiSensors.mclk = addValueSensor(sensors, "mclk", SensorType::FREQUENCY);
  }

  uint64_t power = 0;
  if (rsmi->rsmi_dev_power_ave_get(rsmiIndex, 0, &power) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.power = addValueSensor(sensors, "power_avg", SensorType::POWER);
  }

  uint64_t vram = 0;
  if (rsmi->rsmi_dev_memory_total_get(rsmiIndex, RSMI_MEM_TYPE_VRAM, &vram) ==
      RSMI_STATUS_SUCCESS) {
    rsmiSensors.vramTotal = addValueSensor(sensors, "vram_total", SensorType::MEMORY);
  }
  if (rsmi->rsmi_dev_memory_usage_get(rsmiIndex, RSMI_MEM_TYPE_VRAM, &vram) ==
      RSMI_STATUS_SUCCESS) {
    rsmiSensors.vramUsed = addValueSensor(sensors, "vram_used", SensorType::MEMORY);
  }

  uint64_t tx = 0;
  uint64_t rx = 0;
  if (rsmi->rsmi_dev_pci_throughput_get(rsmiIndex, &tx, &rx, nullptr) == RSMI_STATUS_SUCCESS) {
    rsmiSensors.pcieTx = addValueSensor(sensors, "pcie_tx", SensorType::THROUGHPUT);
    rsmiSensors.pcieRx = addValueSensor(sensors, "pcie_rx", SensorType::THROUGHPUT);
  }

  spdlog::info("using ROCm SMI for {} (index {})", name, rsmiIndex);
  return true;
}

void AmdGpuDevice::readRsmi() {
  if (rsmi == nullptr)
    return;

  if (rsmiSensors.temp != nullptr) {
    int64_t temp = 0;
    if (rsmi->rsmi_dev_temp_metric_get(rsmiIndex, RSMI_TEMP_TYPE_EDGE, RSMI_TEMP_CURRENT, &temp) ==
        RSMI_STATUS_SUCCESS)
      rsmiSensors.temp->setValue(static_cast<long double>(temp) / 1'000); // millidegrees
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
      rsmiSensors.power->setValue(static_cast<long double>(power) / 1'000'000); // microwatts
  }

  if (rsmiSensors.vramTotal != nullptr) {
    uint64_t total = 0;
    if (rsmi->rsmi_dev_memory_total_get(rsmiIndex, RSMI_MEM_TYPE_VRAM, &total) ==
        RSMI_STATUS_SUCCESS)
      rsmiSensors.vramTotal->setValue(static_cast<long double>(total));
  }

  if (rsmiSensors.vramUsed != nullptr) {
    uint64_t used = 0;
    if (rsmi->rsmi_dev_memory_usage_get(rsmiIndex, RSMI_MEM_TYPE_VRAM, &used) ==
        RSMI_STATUS_SUCCESS)
      rsmiSensors.vramUsed->setValue(static_cast<long double>(used));
  }

  if (rsmiSensors.pcieTx != nullptr) {
    uint64_t tx = 0;
    uint64_t rx = 0;
    if (rsmi->rsmi_dev_pci_throughput_get(rsmiIndex, &tx, &rx, nullptr) == RSMI_STATUS_SUCCESS) {
      rsmiSensors.pcieTx->setValue(static_cast<long double>(tx));
      rsmiSensors.pcieRx->setValue(static_cast<long double>(rx));
    }
  }
}

// amdgpu exposes utilization and VRAM usage outside of hwmon, as plain
// integers under the DRM device directory
void AmdGpuDevice::setupSysfs() {
  addSysfsSensor(card.devicePath / "gpu_busy_percent", "gpu_busy", SensorType::UTILIZATION, 1);
  addSysfsSensor(card.devicePath / "mem_busy_percent", "mem_busy", SensorType::UTILIZATION, 1);
  addSysfsSensor(card.devicePath / "mem_info_vram_total", "vram_total", SensorType::MEMORY, 1);
  addSysfsSensor(card.devicePath / "mem_info_vram_used", "vram_used", SensorType::MEMORY, 1);

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
      const SensorType type = helpers::deduceSensorType(it->first);
      const bool keep = type == SensorType::FAN_SPEED || type == SensorType::VOLTAGE;
      it = keep ? std::next(it) : availableSensors.erase(it);
    }
  }

  SharedHwmonParser::createSensors(card.hwmonPath, availableSensors, sensors);
}

void AmdGpuDevice::addSysfsSensor(const fs::path &path, const std::string &sensorName,
                                  SensorType type, unsigned int divider) {
  if (!fs::exists(path))
    return;

  auto stream = std::make_shared<std::ifstream>(path);
  if (!stream->is_open()) {
    spdlog::warn("unable to open {}", path.string());
    return;
  }
  sensors.emplace_back(std::make_unique<Sensor>(std::move(stream), sensorName, type, divider));
}

std::string AmdGpuDevice::sysfsName() const {
  return std::format("AMD GPU ({:04x}:{:04x})", card.vendorId, card.deviceId);
}
