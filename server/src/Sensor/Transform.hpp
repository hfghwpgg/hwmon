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


class TransformDelta : public Transform {
public:
  TransformDelta(unsigned int divider);
  std::optional<double> apply(double raw) override;
  void reset() override;

private:
  double lastValue = NAN;
  std::chrono::steady_clock::time_point lastTime;
};


class TransformScale : public Transform {
public:
  TransformScale(unsigned int divider);
  std::optional<double> apply(double raw) override;
};
