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
  stream.open(streamPath);
}

std::expected<double, Source::SourceStatus> SourceFile::read() {
  stream.clear();
  stream.seekg(0);
  std::string str;
  std::getline(stream, str);

  double readData;
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), readData);
  if (ec != std::errc{} || ptr != str.data() + str.size()) {
    return std::unexpected(SourceStatus::Unreadable);
  }

  return readData;
}
