#include "TransformDelta.hpp"
#include "Sensor/Transform.hpp"
#include <chrono>
#include <cmath>
#include <optional>

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
