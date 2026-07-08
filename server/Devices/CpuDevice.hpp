#pragma once
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

#include "../Device.hpp"

struct lastReading { // cpu time
  unsigned long long totalTime;
  unsigned long long idleTime;
  bool hasRead;
};

struct utilSensorData { // cpu time
  std::shared_ptr<std::stringstream> dataStream;
  lastReading utilOld;
};

class CpuDevice : public Device {
public:
  CpuDevice(std::set<std::filesystem::path> &hwmonPaths);
  ~CpuDevice();

  void initialize() override;

private:
  std::set<std::filesystem::path> &hwmonPaths;
  std::ifstream CPUUTIL_FD;
  std::unordered_map<std::string, utilSensorData> utilSensors;

  void read() override;

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
