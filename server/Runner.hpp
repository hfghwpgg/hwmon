#pragma once
#include "Device.hpp"
#include "SharedState.hpp"
#include <vector>

struct SharedState;

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
