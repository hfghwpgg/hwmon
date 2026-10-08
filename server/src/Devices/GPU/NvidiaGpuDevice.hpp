#pragma once
#include <filesystem>
#include <memory>
#include <set>
#include <string>

#include "Devices/Device.hpp"
#include "GpuDetector.hpp"
#include "Libraries/NvmlLibrary.hpp"
#include "Sensor/Source.hpp"

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
    PushSource *temp = nullptr;
    PushSource *gpuUtil = nullptr;
    PushSource *memUtil = nullptr;
    PushSource *gpuClock = nullptr;
    PushSource *memClock = nullptr;
    PushSource *power = nullptr;
    PushSource *vramTotal = nullptr;
    PushSource *vramUsed = nullptr;
    PushSource *pcieTx = nullptr;
    PushSource *pcieRx = nullptr;
    PushSource *encoderUtil = nullptr;
    PushSource *decoderUtil = nullptr;
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
  [[nodiscard]] std::string sysfsName() const;
};
