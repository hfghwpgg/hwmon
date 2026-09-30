#pragma once

#include <cassert>
#include <chrono>
#include <cmath>
#include <optional>

class Transform {
public:
  virtual ~Transform() = default;
  virtual std::optional<double> apply(double raw) = 0;
  virtual void reset() {}

protected:
  Transform(unsigned int divider) :
      divider(divider) {
    assert(divider != 0);
  }
  unsigned int divider;
};


class DeltaTransform : public Transform {
public:
  DeltaTransform(unsigned int divider);
  std::optional<double> apply(double raw) override;
  void reset() override;

private:
  double lastValue = NAN;
  std::chrono::steady_clock::time_point lastTime;
};


class ScaleTransform : public Transform {
public:
  ScaleTransform(unsigned int divider);
  std::optional<double> apply(double raw) override;
};
