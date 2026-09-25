#include "PreadFile.hpp"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <utility>

namespace {
// First read is this big, then the buffer doubles until the file fits or the
// caller's ceiling is hit. Small enough that a sysfs attribute does not pay
// for a multi-kilobyte zeroed allocation.
constexpr std::size_t initialChunk = 256;
} // namespace

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

  fd = std::move(opened);
  capacity_ = capacity;
  highWater_ = 0;
  buffer.clear();
  buffer.shrink_to_fit();
  return true;
}

bool PreadFile::isOpen() const noexcept {
  return fd.valid();
}

std::expected<std::string_view, std::errc> PreadFile::read() {
  if (!fd.valid()) {
    return std::unexpected(std::errc::bad_file_descriptor);
  }

  // Reserve a small window only. The ceiling stays unallocated until a read
  // actually needs it, and is released again once we know the high-water mark.
  if (buffer.empty() && capacity_ > 0) {
    buffer.resize(std::min(initialChunk, capacity_));
  }

  std::size_t total = 0;
  while (total < capacity_) {
    if (total == buffer.size()) {
      // Exact fit (steady state after shrinking to the file size). One extra
      // byte tells us whether to grow; a short file must not allocate again.
      char extra{};
      ssize_t more = 0;
      do {
        more = ::pread(fd.get(), &extra, 1, static_cast<off_t>(total));
      } while (more < 0 && errno == EINTR);
      if (more < 0) {
        return std::unexpected(static_cast<std::errc>(errno));
      }
      if (more == 0) {
        break;
      }

      const std::size_t doubled = buffer.size() <= capacity_ / 2 ? buffer.size() * 2 : capacity_;
      const std::size_t grown = std::max(doubled, std::min(initialChunk, capacity_));
      buffer.resize(std::min(capacity_, std::max(grown, total + 1)));
      buffer[total] = extra;
      total += 1;
      continue;
    }

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

  if (total == capacity_) {
    char extra{};
    ssize_t more = 0;
    do {
      more = ::pread(fd.get(), &extra, 1, static_cast<off_t>(total));
    } while (more < 0 && errno == EINTR);
    if (more > 0) {
      spdlog::warn("PreadFile: contents exceed {} byte buffer; truncating", capacity_);
    }
  }

  if (total > highWater_) {
    highWater_ = total;
  }
  // Drop doubled slack so the resident buffer matches the most we've read.
  if (highWater_ < buffer.capacity()) {
    std::vector<char> fitted(buffer.begin(),
                             buffer.begin() + static_cast<std::ptrdiff_t>(highWater_));
    buffer.swap(fitted);
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
