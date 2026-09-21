#include "SourcePush.hpp"
#include <cmath>
#include <expected>
#include <optional>
#include <utility>

void SourcePush::setValue(double value) {
  pending = value;
}

void SourcePush::invalidate() {
  pending = NAN;
}

void SourcePush::reset() {
  pending = NAN;
}

std::expected<double, Source::SourceStatus> SourcePush::read() {
  if (std::isnan(pending))
    return std::unexpected(Source::SourceStatus::NotReady);

  return std::exchange(pending, NAN);
}
