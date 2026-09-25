#pragma once

#include <expected>
#include <filesystem>

#include "PreadFile.hpp"
#include "Source.hpp"

class SourceFile : public Source {
public:
  SourceFile(const std::filesystem::path &streamPath);
  std::expected<double, SourceStatus> read() override;

private:
  PreadFile file;
};
