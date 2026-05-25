#pragma once
#include "Device.hpp"
#include <vector>

using std::vector;

class Runner {
public:
  Runner();
  ~Runner();
  void Setup();
  void Run();

private:
  vector<Device> Devices;
};