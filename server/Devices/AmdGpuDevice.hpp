#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>

#include "../Device.hpp"
#include "../Libraries/RsmiLibrary.hpp"
#include "../ValueSensor.hpp"
#include "GpuDetector.hpp"

class AmdGpuDevice : public Device {
public:
  // hwmonPaths is the set Runner hands out; the card's own hwmon directory is
  // removed from it so it doesn't show up again as a GeneralDevice
  AmdGpuDevice(GpuCardInfo card, std::set<std::filesystem::path> &hwmonPaths);
  // allowRsmi = false forces the sysfs backend, used by the tests
  AmdGpuDevice(GpuCardInfo card, std::set<std::filesystem::path> &hwmonPaths, bool allowRsmi);

  void initialize() override;
  void read() override;

private:
  // ROCm SMI reports everything through library calls, so each metric gets a
  // ValueSensor; a null pointer means the card doesn't support that metric
  struct RsmiSensors {
    ValueSensor *temp = nullptr;
    ValueSensor *gpuBusy = nullptr;
    ValueSensor *memBusy = nullptr;
    ValueSensor *sclk = nullptr;
    ValueSensor *mclk = nullptr;
    ValueSensor *power = nullptr;
    ValueSensor *vramTotal = nullptr;
    ValueSensor *vramUsed = nullptr;
    ValueSensor *pcieTx = nullptr;
    ValueSensor *pcieRx = nullptr;
  };

  const GpuCardInfo card;
  std::set<std::filesystem::path> &hwmonPaths;
  const bool allowRsmi;

  std::shared_ptr<RsmiLibrary> rsmi;
  uint32_t rsmiIndex = 0;
  RsmiSensors rsmiSensors;

  bool setupRsmi();
  void setupSysfs();
  void readRsmi();

  // adds the card's hwmon sensors; when onlyUncoveredMetrics is set, only the
  // readings ROCm SMI doesn't provide (fan speed, voltage) are added
  void addHwmonSensors(bool onlyUncoveredMetrics);
  void addSysfsSensor(const std::filesystem::path &path, const std::string &sensorName,
                      SensorType type, unsigned int divider);
  std::string sysfsName() const;
};
