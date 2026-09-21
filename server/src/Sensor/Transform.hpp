#pragma once

#include <cassert>
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
