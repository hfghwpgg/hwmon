#pragma once

#include <expected>
#include <filesystem>
#include <fstream>

#include "Source.hpp"

class SourceFile : public Source {
public:
  SourceFile(const std::filesystem::path &streamPath);
  ~SourceFile();
  std::expected<double, SourceStatus> read() override;

private:
  int fd;
};
