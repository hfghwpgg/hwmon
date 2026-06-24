#pragma once
#include "../Device.hpp"

class CpuDevice : public Device {
public:
  CpuDevice();
  ~CpuDevice();

private:
  void getCpuCoreFrequency();
};
