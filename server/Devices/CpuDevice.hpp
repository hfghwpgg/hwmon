#pragma once
#include "../Device.hpp"
#include <string>
#include <unordered_map>
#include <fstream>

struct lastReading { // cpu time
  unsigned long long totalTime;
  unsigned long idleTime;
  bool hasRead;
};

class CpuDevice : public Device {
public:
  CpuDevice();
  ~CpuDevice();

  void initialize() override;

private:
  std::ifstream CPUUTIL_FD;
  std::unordered_map<std::string, std::shared_ptr<std::stringstream>> utilSensorDescriptors;
  std::unordered_map<std::string, lastReading> utilOld;

  void read() override;

  void getCpuCoreFrequency();
  void getCpuUtilization();
  void readCpuUtilization();
  std::string getCpuName();
};
