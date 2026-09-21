#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <set>

#include "Device.hpp"
#include "GpuDetector.hpp"
#include "Libraries/RsmiLibrary.hpp"
#include "Sensor/SourcePush.hpp"
#include "helpers.hpp"

class AmdGpuDevice : public Device {
public:
  // hwmonPaths is the set Runner hands out; the card's own hwmon directory is
  // removed from it so it doesn't show up again as a GeneralDevice
  AmdGpuDevice(GpuCardInfo card, std::set<std::filesystem::path> &hwmonPaths);
  // allowRsmi = false forces the sysfs backend, used by the tests
  AmdGpuDevice(GpuCardInfo card, std::set<std::filesystem::path> &hwmonPaths, bool allowRsmi);

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  // ROCm SMI reports everything through library calls, so each metric gets a
  // SourcePush; a null pointer means the card doesn't support that metric
  struct RsmiSensors {
    SourcePush *temp_edge = nullptr;
    SourcePush *temp_junction = nullptr;
    SourcePush *temp_vram = nullptr;
    SourcePush *gpuBusy = nullptr;
    SourcePush *memBusy = nullptr;
    SourcePush *sclk = nullptr;
    SourcePush *mclk = nullptr;
    SourcePush *power = nullptr;
    SourcePush *powerCap = nullptr;
    SourcePush *vramTotal = nullptr;
    SourcePush *vramUsed = nullptr;
    SourcePush *pcieTx = nullptr;
    SourcePush *pcieRx = nullptr;
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
  void addSysfsSensor(helpers::SensorVec &sensors, const std::filesystem::path &path,
                      const std::string &sensorName, SensorType type, unsigned int divider,
                      bool aggregateData = true, bool isPrimary = false);
  std::string sysfsName() const;
};
