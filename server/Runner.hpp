#pragma once
#include "Device.hpp"
#include "SharedState.hpp"
#include <vector>

class Runner {
public:
  explicit Runner(SharedState &state);
  ~Runner();
  void Run();

private:
  SharedState &state;
  std::vector<Device> devices;
  void Setup();
};
