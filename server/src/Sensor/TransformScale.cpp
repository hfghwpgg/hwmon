#include "TransformScale.hpp"
#include "Sensor/Transform.hpp"

TransformScale::TransformScale(unsigned int divider) :
    Transform(divider) {}

// this shouldnt break, so it never returns nullopt
std::optional<double> TransformScale::apply(double raw) {
  return raw / divider;
}
