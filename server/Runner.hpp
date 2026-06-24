#pragma once
#include <memory>
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
  std::vector<std::unique_ptr<Device>> devices;
  void setup();
};
