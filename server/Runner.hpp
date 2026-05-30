#pragma once
#include "Device.hpp"
#include <atomic>
#include <vector>

class Runner {
public:
  Runner();
  ~Runner();
  void Run();
  static void Stop(int sig);

private:
  static std::atomic<bool> running;
  std::vector<Device> Devices;
  void Setup();
};