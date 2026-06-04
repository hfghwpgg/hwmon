#pragma once
#include "Device.hpp"
#include <atomic>
#include <vector>

class Runner {
public:
  Runner(unsigned int intervalMs);
  ~Runner();
  void Run();
  static void Interrupt(int sig);

private:
  static std::atomic<bool> running;
  std::vector<Device> devices;
  unsigned int intervalMs;
  void Setup();
};
