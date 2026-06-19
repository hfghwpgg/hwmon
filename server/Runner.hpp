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

  void run();

private:
  SharedState &state;
  std::vector<Device> devices;
  void setup();
};
