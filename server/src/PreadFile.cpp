#include "PreadFile.hpp"

#include <cerrno>
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <utility>

PreadFile::PreadFile(const std::filesystem::path &path, std::size_t capacity) {
  open(path, capacity);
}

bool PreadFile::open(const std::filesystem::path &path, std::size_t capacity) {
  if (capacity == 0) {
    capacity = defaultCapacity;
  }

  FdGuard opened{::open(path.c_str(), O_RDONLY | O_CLOEXEC)};
  if (!opened.valid()) {
    return false;
  }

  std::vector<char> storage(capacity);
  fd = std::move(opened);
  buffer = std::move(storage);
  return true;
}

bool PreadFile::isOpen() const noexcept {
  return fd.valid();
}

std::expected<std::string_view, std::errc> PreadFile::read() {
  if (!fd.valid()) {
    return std::unexpected(std::errc::bad_file_descriptor);
  }

  std::size_t total = 0;
  while (total < buffer.size()) {
    const ssize_t n =
        ::pread(fd.get(), buffer.data() + total, buffer.size() - total, static_cast<off_t>(total));
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return std::unexpected(static_cast<std::errc>(errno));
    }
    if (n == 0) {
      break;
    }
    total += static_cast<std::size_t>(n);
  }

  if (total == buffer.size()) {
    char extra{};
    ssize_t more = 0;
    do {
      more = ::pread(fd.get(), &extra, 1, static_cast<off_t>(total));
    } while (more < 0 && errno == EINTR);
    if (more > 0) {
      spdlog::warn("PreadFile: contents exceed {} byte buffer; truncating", buffer.size());
    }
  }

  return std::string_view{buffer.data(), total};
}

std::expected<std::string_view, std::errc> PreadFile::readFirstLine() {
  const auto text = read();
  if (!text) {
    return std::unexpected(text.error());
  }

  std::size_t offset = 0;
  const auto line = nextLine(*text, offset);
  if (!line) {
    return std::string_view{};
  }
  return *line;
}

std::optional<std::string_view> PreadFile::nextLine(std::string_view text, std::size_t &offset) {
  if (offset >= text.size()) {
    return std::nullopt;
  }

  const auto newline = text.find('\n', offset);
  const auto end = newline == std::string_view::npos ? text.size() : newline;
  const std::string_view line = text.substr(offset, end - offset);
  offset = end < text.size() ? end + 1 : text.size();
  return line;
}
