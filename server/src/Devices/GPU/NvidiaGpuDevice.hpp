#pragma once
#include <filesystem>
#include <memory>
#include <set>
#include <string>

#include "Device.hpp"
#include "GpuDetector.hpp"
#include "Libraries/NvmlLibrary.hpp"
#include "Sensor/SourcePush.hpp"
#include "helpers.hpp"

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
  // SourcePush; a null pointer means the card doesn't support that metric
  struct NvmlSensors {
    SourcePush *temp = nullptr;
    SourcePush *gpuUtil = nullptr;
    SourcePush *memUtil = nullptr;
    SourcePush *gpuClock = nullptr;
    SourcePush *memClock = nullptr;
    SourcePush *power = nullptr;
    SourcePush *vramTotal = nullptr;
    SourcePush *vramUsed = nullptr;
    SourcePush *pcieTx = nullptr;
    SourcePush *pcieRx = nullptr;
    SourcePush *encoderUtil = nullptr;
    SourcePush *decoderUtil = nullptr;
  };

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
