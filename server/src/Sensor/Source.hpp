#pragma once
#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>

class Source {
public:
  enum class SourceStatus { NotReady, Unreadable };
  virtual ~Source() = default;
  virtual std::expected<double, SourceStatus> read() = 0;
  // drops any value buffered for the reporting window that just ended
  virtual void reset() {}
};


class FileSource : public Source {
public:
  FileSource(const std::filesystem::path &streamPath);
  std::expected<double, SourceStatus> read() override;

private:
  std::ifstream stream;
};


class PushSource : public Source {
public:
  void setValue(double value);
  void invalidate();
  std::expected<double, SourceStatus> read() override;
  void reset() override;

private:
  double pending = NAN;
};
