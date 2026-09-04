#include "NvidiaGpuDevice.hpp"

#include <filesystem>
#include <format>
#include <memory>
#include <set>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

#include "../Device.hpp"
#include "../Libraries/NvmlLibrary.hpp"
#include "../SensorType.hpp"
#include "../ValueSensor.hpp"
#include "../helpers.hpp"
#include "GpuDetector.hpp"
#include "SharedHwmonParser.hpp"

namespace fs = std::filesystem;

namespace {

// NVML device names carry marketing decorations that only add noise here
std::string stripBranding(std::string deviceName) {
  for (const auto *brand : {"NVIDIA", "Nvidia", "(R)", "(TM)"}) {
    for (auto pos = deviceName.find(brand); pos != std::string::npos;
         pos = deviceName.find(brand)) {
      deviceName.erase(pos, std::string(brand).size());
    }
  }
  return helpers::trim(deviceName);
}

} // namespace

NvidiaGpuDevice::NvidiaGpuDevice(GpuCardInfo card, std::set<fs::path> &hwmonPaths) :
    NvidiaGpuDevice(std::move(card), hwmonPaths, true) {}

NvidiaGpuDevice::NvidiaGpuDevice(GpuCardInfo card, std::set<fs::path> &hwmonPaths, bool allowNvml) :
    Device(card.cardPath.filename().string(), DeviceType::GPU),
    card(std::move(card)),
    hwmonPaths(hwmonPaths),
    allowNvml(allowNvml) {}

void NvidiaGpuDevice::initialize() {
  if (!card.hwmonPath.empty())
    hwmonPaths.erase(card.hwmonPath);

  name = sysfsName();
  const bool haveNvml = allowNvml && setupNvml();

  // the proprietary driver exposes no hwmon node, so this only contributes
  // under nouveau, where NVML is unavailable anyway
  addHwmonSensors();

  if (!haveNvml && sensors.empty())
    spdlog::warn("{}: no readable metrics found", card.devicePath.string());
}

void NvidiaGpuDevice::read() {
  readNvml();
  Device::read();
}

// Probes every metric once and only creates sensors for the ones the card
// answers, mirroring btop's supported_functions handling.
bool NvidiaGpuDevice::setupNvml() {
  nvml = NvmlLibrary::acquire();
  if (nvml == nullptr)
    return false;

  if (!nvml->getHandleByPciAddress(card.pciAddress, handle)) {
    nvml.reset();
    return false;
  }

  char deviceName[NVML_DEVICE_NAME_BUFFER_SIZE];
  if (nvml->nvmlDeviceGetName(handle, deviceName, NVML_DEVICE_NAME_BUFFER_SIZE) == NVML_SUCCESS) {
    name = stripBranding(deviceName);
  } else {
    spdlog::warn("NVML: failed to get device name for {}", card.pciAddress);
  }

  unsigned int value = 0;
  if (nvml->nvmlDeviceGetTemperature(handle, NVML_TEMPERATURE_GPU, &value) == NVML_SUCCESS) {
    nvmlSensors.temp = addValueSensor(sensors, "gpu", SensorType::TEMPERATURE);
  }

  nvmlUtilization_t utilization{};
  if (nvml->nvmlDeviceGetUtilizationRates(handle, &utilization) == NVML_SUCCESS) {
    nvmlSensors.gpuUtil = addValueSensor(sensors, "gpu_util", SensorType::UTILIZATION);
    nvmlSensors.memUtil = addValueSensor(sensors, "mem_util", SensorType::UTILIZATION);
  }

  if (nvml->nvmlDeviceGetClockInfo(handle, NVML_CLOCK_GRAPHICS, &value) == NVML_SUCCESS) {
    nvmlSensors.gpuClock = addValueSensor(sensors, "gpu_clock", SensorType::FREQUENCY);
  }
  if (nvml->nvmlDeviceGetClockInfo(handle, NVML_CLOCK_MEM, &value) == NVML_SUCCESS) {
    nvmlSensors.memClock = addValueSensor(sensors, "mem_clock", SensorType::FREQUENCY);
  }

  if (nvml->nvmlDeviceGetPowerUsage(handle, &value) == NVML_SUCCESS) {
    nvmlSensors.power = addValueSensor(sensors, "power", SensorType::POWER);
  }

  nvmlMemory_t memory{};
  if (nvml->nvmlDeviceGetMemoryInfo(handle, &memory) == NVML_SUCCESS) {
    nvmlSensors.vramTotal = addValueSensor(sensors, "vram_total", SensorType::MEMORY);
    nvmlSensors.vramUsed = addValueSensor(sensors, "vram_used", SensorType::MEMORY);
  }

  if (nvml->nvmlDeviceGetPcieThroughput(handle, NVML_PCIE_UTIL_TX_BYTES, &value) == NVML_SUCCESS) {
    nvmlSensors.pcieTx = addValueSensor(sensors, "pcie_tx", SensorType::THROUGHPUT);
    nvmlSensors.pcieRx = addValueSensor(sensors, "pcie_rx", SensorType::THROUGHPUT);
  }

  unsigned int samplingPeriodUs = 0;
  if (nvml->nvmlDeviceGetEncoderUtilization(handle, &value, &samplingPeriodUs) == NVML_SUCCESS) {
    nvmlSensors.encoderUtil = addValueSensor(sensors, "encoder_util", SensorType::UTILIZATION);
  }
  if (nvml->nvmlDeviceGetDecoderUtilization(handle, &value, &samplingPeriodUs) == NVML_SUCCESS) {
    nvmlSensors.decoderUtil = addValueSensor(sensors, "decoder_util", SensorType::UTILIZATION);
  }

  spdlog::info("using NVML for {}", name);
  return true;
}

void NvidiaGpuDevice::readNvml() {
  if (nvml == nullptr)
    return;

  if (nvmlSensors.temp != nullptr) {
    unsigned int temp = 0;
    if (nvml->nvmlDeviceGetTemperature(handle, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS)
      nvmlSensors.temp->setValue(temp);
  }

  if (nvmlSensors.gpuUtil != nullptr) {
    nvmlUtilization_t utilization{};
    if (nvml->nvmlDeviceGetUtilizationRates(handle, &utilization) == NVML_SUCCESS) {
      nvmlSensors.gpuUtil->setValue(utilization.gpu);
      nvmlSensors.memUtil->setValue(utilization.memory);
    }
  }

  if (nvmlSensors.gpuClock != nullptr) {
    unsigned int clock = 0;
    if (nvml->nvmlDeviceGetClockInfo(handle, NVML_CLOCK_GRAPHICS, &clock) == NVML_SUCCESS)
      nvmlSensors.gpuClock->setValue(clock);
  }

  if (nvmlSensors.memClock != nullptr) {
    unsigned int clock = 0;
    if (nvml->nvmlDeviceGetClockInfo(handle, NVML_CLOCK_MEM, &clock) == NVML_SUCCESS)
      nvmlSensors.memClock->setValue(clock);
  }

  if (nvmlSensors.power != nullptr) {
    unsigned int power = 0;
    if (nvml->nvmlDeviceGetPowerUsage(handle, &power) == NVML_SUCCESS)
      nvmlSensors.power->setValue(static_cast<long double>(power) / 1'000); // milliwatts
  }

  if (nvmlSensors.vramTotal != nullptr) {
    nvmlMemory_t memory{};
    if (nvml->nvmlDeviceGetMemoryInfo(handle, &memory) == NVML_SUCCESS) {
      nvmlSensors.vramTotal->setValue(static_cast<long double>(memory.total));
      nvmlSensors.vramUsed->setValue(static_cast<long double>(memory.used));
    }
  }

  // each of these calls blocks for ~20ms inside the driver
  if (nvmlSensors.pcieTx != nullptr) {
    unsigned int tx = 0;
    unsigned int rx = 0;
    if (nvml->nvmlDeviceGetPcieThroughput(handle, NVML_PCIE_UTIL_TX_BYTES, &tx) == NVML_SUCCESS)
      nvmlSensors.pcieTx->setValue(static_cast<long double>(tx) * 1'024); // reported in KB/s
    if (nvml->nvmlDeviceGetPcieThroughput(handle, NVML_PCIE_UTIL_RX_BYTES, &rx) == NVML_SUCCESS)
      nvmlSensors.pcieRx->setValue(static_cast<long double>(rx) * 1'024);
  }

  if (nvmlSensors.encoderUtil != nullptr) {
    unsigned int utilization = 0;
    unsigned int samplingPeriodUs = 0;
    if (nvml->nvmlDeviceGetEncoderUtilization(handle, &utilization, &samplingPeriodUs) ==
        NVML_SUCCESS)
      nvmlSensors.encoderUtil->setValue(utilization);
  }

  if (nvmlSensors.decoderUtil != nullptr) {
    unsigned int utilization = 0;
    unsigned int samplingPeriodUs = 0;
    if (nvml->nvmlDeviceGetDecoderUtilization(handle, &utilization, &samplingPeriodUs) ==
        NVML_SUCCESS)
      nvmlSensors.decoderUtil->setValue(utilization);
  }
}

void NvidiaGpuDevice::addHwmonSensors() {
  if (card.hwmonPath.empty())
    return;

  const auto availableSensors = SharedHwmonParser::parseHwmonDirectory(card.hwmonPath);
  SharedHwmonParser::createSensors(card.hwmonPath, availableSensors, sensors);
}

std::string NvidiaGpuDevice::sysfsName() const {
  return std::format("NVIDIA GPU ({:04x}:{:04x})", card.vendorId, card.deviceId);
}
