#pragma once
#include "../Device.hpp"
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <fstream>

struct lastReading { // cpu time
  unsigned long long totalTime;
  unsigned long long idleTime;
  bool hasRead;
};

struct utilSensorData {
  std::shared_ptr<std::stringstream> dataStream;
  lastReading utilOld;
};

class CpuDevice : public Device {
public:
  CpuDevice();
  ~CpuDevice();

  void initialize() override;

private:
  std::ifstream CPUUTIL_FD;
  std::unordered_map<std::string, utilSensorData> utilSensors;

  void read() override;

  void getCpuCoreFrequency();
  void initCpuUtilization();
  void readCpuUtilization();
  std::string getCpuName();
};
