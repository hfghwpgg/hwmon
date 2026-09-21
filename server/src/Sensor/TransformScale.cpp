#include "TransformScale.hpp"
#include "Sensor/Transform.hpp"

TransformScale::TransformScale(unsigned int divider) :
    Transform(divider) {}

std::optional<double> TransformScale::apply(double raw) {
  return raw / divider;
}
