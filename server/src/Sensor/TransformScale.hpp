#pragma once
#include "Transform.hpp"

class TransformScale : public Transform {
public:
  TransformScale(unsigned int divider);
  std::optional<double> apply(double raw) override;
};
