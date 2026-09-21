#pragma once
#include "Source.hpp"
#include <cmath>

class SourcePush : public Source {
public:
  void setValue(double value);
  void invalidate();
  std::expected<double, SourceStatus> read() override;
  void reset() override;

private:
  double pending = NAN;
};
