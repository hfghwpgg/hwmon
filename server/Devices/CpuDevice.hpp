#pragma once
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Device.hpp"
#include "../ValueSensor.hpp"

struct lastReading { // cpu time
  unsigned long long totalTime;
  unsigned long long idleTime;
  bool hasRead;
};

struct utilSensorData { // cpu time
  ValueSensor *sensor;
  lastReading utilOld;
};

class CpuDevice : public Device {
public:
  CpuDevice(std::set<std::filesystem::path> &hwmonPaths);
  CpuDevice(std::set<std::filesystem::path> &hwmonPaths, std::filesystem::path CPUFREQ_PATH,
            std::filesystem::path CPUINFO_PATH, std::filesystem::path CPUUTIL_PATH);
  ~CpuDevice();

  void initialize() override;
  void read() override;
  void resetReadings() override;

private:
  std::unordered_map<std::string, utilSensorData> utilSensors;

  const std::filesystem::path CPUFREQ_PATH;
  const std::filesystem::path CPUINFO_PATH;
  const std::filesystem::path CPUUTIL_PATH;

  std::set<std::filesystem::path> &hwmonPaths;
  std::ifstream CPUUTIL_FD;

  // temp
  void getTemperature();

  // clocks
  void getCoreFrequency();

  // cpu name
  std::string getName();

  // utilization
  void initUtilization();
  void readUtilization();

  // TODO:
  //       power
  //       vcore
};
