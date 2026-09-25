#include "SourceFile.hpp"
#include "helpers.hpp"
#include <charconv>
#include <format>
#include <stdexcept>
#include <unistd.h>

SourceFile::SourceFile(const std::filesystem::path &streamPath) {
  if (helpers::pathType(streamPath) != helpers::pathTypeEnum::FILE ||
      access(streamPath.c_str(), R_OK) == -1 || !file.open(streamPath)) {
    throw std::runtime_error(
        std::format("SourceFile: path is invalid or inaccessible ({})", streamPath.string()));
  }
}

std::expected<double, Source::SourceStatus> SourceFile::read() {
  const auto line = file.readFirstLine();
  if (!line) {
    return std::unexpected(SourceStatus::Unreadable);
  }

  double readData;
  const auto [ptr, ec] = std::from_chars(line->data(), line->data() + line->size(), readData);
  if (ec != std::errc{} || ptr != line->data() + line->size()) {
    return std::unexpected(SourceStatus::Unreadable);
  }

  return readData;
}
