#pragma once
#include <vector>

#include "Device.hpp"
#include "SharedState.hpp"

struct SharedState;

class Runner {
public:
  explicit Runner(SharedState &state);
#ifdef DEBUG
  ~Runner();
#endif
  void Run();

private:
  SharedState &state;
  std::vector<Device> devices;
  void Setup();
};
