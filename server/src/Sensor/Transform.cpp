#include "Transform.hpp"

#include <chrono>
#include <cmath>
#include <optional>

// ***************
// ***  Delta  ***
// ***************
DeltaTransform::DeltaTransform(unsigned int divider) :
    Transform(divider) {}

std::optional<double> DeltaTransform::apply(double raw) {
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

void DeltaTransform::reset() {
  lastTime = std::chrono::steady_clock::now();
  lastValue = NAN;
}


// ***************
// ***  Scale  ***
// ***************
ScaleTransform::ScaleTransform(unsigned int divider) :
    Transform(divider) {}

// this shouldnt break, so it never returns nullopt
std::optional<double> ScaleTransform::apply(double raw) {
  return raw / divider;
}
