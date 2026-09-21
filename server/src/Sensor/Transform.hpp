#pragma once

#include <cassert>
#include <optional>
class Transform {
public:
  Transform(unsigned int divider) :
      divider(divider) {
    assert(divider != 0);
  }
  virtual ~Transform() = default;
  virtual std::optional<double> apply(double raw) = 0;
  virtual void reset() {}

protected:
  unsigned int divider;
};
