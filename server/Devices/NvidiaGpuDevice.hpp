#pragma once
#include <filesystem>
#include <memory>
#include <set>
#include <string>

#include "../Device.hpp"
#include "../Libraries/NvmlLibrary.hpp"
#include "../ValueSensor.hpp"
#include "GpuDetector.hpp"

class NvidiaGpuDevice : public Device {
public:
  // hwmonPaths is the set Runner hands out; the card's own hwmon directory is
  // removed from it so it doesn't show up again as a GeneralDevice
  NvidiaGpuDevice(GpuCardInfo card, std::set<std::filesystem::path> &hwmonPaths);
  // allowNvml = false forces the hwmon backend, used by the tests
  NvidiaGpuDevice(GpuCardInfo card, std::set<std::filesystem::path> &hwmonPaths, bool allowNvml);

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  // NVML reports everything through library calls, so each metric gets a
  // ValueSensor; a null pointer means the card doesn't support that metric
  struct NvmlSensors {
    ValueSensor *temp = nullptr;
    ValueSensor *gpuUtil = nullptr;
    ValueSensor *memUtil = nullptr;
    ValueSensor *gpuClock = nullptr;
    ValueSensor *memClock = nullptr;
    ValueSensor *power = nullptr;
    ValueSensor *vramTotal = nullptr;
    ValueSensor *vramUsed = nullptr;
    ValueSensor *pcieTx = nullptr;
    ValueSensor *pcieRx = nullptr;
    ValueSensor *encoderUtil = nullptr;
    ValueSensor *decoderUtil = nullptr;
  };

  std::vector<std::unique_ptr<Sensor>> tempSensors;
  std::vector<std::unique_ptr<Sensor>> utilizationSensors;
  std::vector<std::unique_ptr<Sensor>> memSensors;
  std::vector<std::unique_ptr<Sensor>> pcieTxRx;
  std::vector<std::unique_ptr<Sensor>> sysfsFallback;

  const GpuCardInfo card;
  std::set<std::filesystem::path> &hwmonPaths;
  const bool allowNvml;

  std::shared_ptr<NvmlLibrary> nvml;
  nvmlDevice_t handle = nullptr;
  NvmlSensors nvmlSensors;

  bool setupNvml();
  void readNvml();
  void addHwmonSensors();
  std::string sysfsName() const;
};
