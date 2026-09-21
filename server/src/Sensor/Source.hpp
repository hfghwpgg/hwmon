#pragma once
#include <expected>

class Source {
public:
  enum class SourceStatus { NotReady, Unreadable };
  virtual ~Source() = default;
  virtual std::expected<double, SourceStatus> read() = 0;
  // drops any value buffered for the reporting window that just ended
  virtual void reset() {}
};
