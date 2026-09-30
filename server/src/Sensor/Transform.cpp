#include "Transform.hpp"

#include <optional>
#include <chrono>
#include <cmath>

// ***************
// ***  Delta  ***
// ***************
TransformDelta::TransformDelta(unsigned int divider) :
    Transform(divider) {}

std::optional<double> TransformDelta::apply(double raw) {
  if (std::isnan(lastValue)) {
    lastValue = raw;
    lastTime = std::chrono::steady_clock::now();
    return std::nullopt;
  }

  auto time = std::chrono::steady_clock::now();
  auto deltaTime = std::chrono::duration<double>(time - lastTime).count();
  double ret = (raw - lastValue) / (deltaTime * divider);

  lastValue = raw;
  lastTime = time;
  return ret;
}

void TransformDelta::reset() {
  lastTime = std::chrono::steady_clock::now();
  lastValue = NAN;
}


// ***************
// ***  Scale  ***
// ***************
TransformScale::TransformScale(unsigned int divider) :
    Transform(divider) {}

// this shouldnt break, so it never returns nullopt
std::optional<double> TransformScale::apply(double raw) {
  return raw / divider;
}
