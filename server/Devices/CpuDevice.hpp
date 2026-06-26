#pragma once
#include "../Device.hpp"
#include <string>

class CpuDevice : public Device {
public:
  CpuDevice();
  ~CpuDevice();

private:
  void getCpuCoreFrequency();
  // cpu utilisation is not compatible with current sensor
  // reading logic so i wont bother with it for now
  // void getCpuUtilization();
  std::string getCpuName();
};
