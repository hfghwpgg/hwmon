#pragma once
#include "Transform.hpp"
#include <chrono>
#include <cmath>

class TransformDelta : public Transform {
public:
  TransformDelta(unsigned int divider);
  std::optional<double> apply(double raw) override;
  void reset() override;

private:
  double lastValue = NAN;
  std::chrono::steady_clock::time_point lastTime;
};
