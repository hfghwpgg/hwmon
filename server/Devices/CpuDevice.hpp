#pragma once
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <unordered_map>

#include "../Device.hpp"
#include "../ValueSensor.hpp"
#include "../hwmon.hpp"


class CpuDevice : public Device {
public:
  CpuDevice(std::set<hwmon::fs::path> &hwmonPaths);
  CpuDevice(std::set<hwmon::fs::path> &hwmonPaths, hwmon::fs::path cpufreq_path,
            hwmon::fs::path cpuinfo_path, hwmon::fs::path cpuutil_path,
            hwmon::fs::path intelrapl_path);
  ~CpuDevice();

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  struct lastReading { // cpu time
    unsigned long long totalTime;
    unsigned long long idleTime;
    bool hasRead;
  };

  struct utilSensorData { // cpu time
    ValueSensor *sensor;
    lastReading utilOld;
  };

  const struct CpuPaths {
    hwmon::fs::path cpufreq;
    hwmon::fs::path cpuinfo;
    hwmon::fs::path cpuutil;
    hwmon::fs::path intelrapl;
  } cpuPaths;

  std::unordered_map<std::string, utilSensorData> utilSensorsPrivate;

  std::set<hwmon::fs::path> &hwmonPaths;
  std::ifstream cpuutil_fd;

  void getTemperature();
  void getCoreFrequency();
  std::string getName();

  void initUtilization();
  void readUtilization();

  void getPowerDraw();

  // TODO:
  //       power
  //       vcore
};
