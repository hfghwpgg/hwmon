#pragma once
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <unordered_map>

#include "../Device.hpp"
#include "../ValueSensor.hpp"
#include "../helpers.hpp"


class CpuDevice : public Device {
public:
  CpuDevice(std::set<helpers::fs::path> &hwmonPaths);
  CpuDevice(std::set<helpers::fs::path> &hwmonPaths, helpers::fs::path cpufreq_path,
            helpers::fs::path cpuinfo_path, helpers::fs::path cpuutil_path,
            helpers::fs::path intelrapl_path);
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
    helpers::fs::path cpufreq;
    helpers::fs::path cpuinfo;
    helpers::fs::path cpuutil;
    helpers::fs::path intelrapl;
  } cpuPaths;

  std::unordered_map<std::string, utilSensorData> utilSensorsPrivate;

  std::set<helpers::fs::path> &hwmonPaths;
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
