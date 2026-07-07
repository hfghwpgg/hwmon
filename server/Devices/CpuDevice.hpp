#pragma once
#include "../Device.hpp"
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <fstream>
#include <vector>

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
  CpuDevice(std::vector<std::filesystem::path> &hwmonPaths);
  ~CpuDevice();

  void initialize() override;

private:
  std::vector<std::filesystem::path> &hwmonPaths;
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
