#include "SourceFile.hpp"
#include "helpers.hpp"
#include <charconv>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <unistd.h>

SourceFile::SourceFile(const std::filesystem::path &streamPath) {
  if (helpers::pathType(streamPath) != helpers::pathTypeEnum::FILE ||
      access(streamPath.c_str(), R_OK) == -1) {
    throw std::runtime_error(
        std::format("SourceFile: path is invalid or inaccessible ({})", streamPath.string()));
  }
  fd = open(streamPath.c_str(), O_RDONLY | O_CLOEXEC);
}

SourceFile::~SourceFile() {
  if (fd >= 0)
    close(fd);
}

std::expected<double, Source::SourceStatus> SourceFile::read() {
  char buf[64];
  ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
  if (n <= 0)
    return std::unexpected(SourceStatus::Unreadable);

  long readData;
  auto [ptr, ec] = std::from_chars(buf, buf + n, readData);
  if (ec != std::errc{} || ptr == buf)
    return std::unexpected(SourceStatus::Unreadable);

  while (ptr != buf + n && (*ptr == '\n' || *ptr == '\r' || *ptr == ' ' || *ptr == '\t'))
    ++ptr;
  if (ptr != buf + n)
    return std::unexpected(SourceStatus::Unreadable);

  return static_cast<double>(readData);
}
