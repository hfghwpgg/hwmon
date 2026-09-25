#pragma once

#include "FdGuard.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

// Re-reads a sysfs or proc file with pread() from offset 0. The descriptor
// stays open so callers can sample the same file without reopening it.
// Views returned by read() and readFirstLine() point into an internal buffer
// and are invalidated by the next read, open, move, or destruction.
class PreadFile {
public:
  // Hwmon attributes fit in a page. Multi-line proc files need more room.
  static constexpr std::size_t defaultCapacity = 4096;
  static constexpr std::size_t largeCapacity = 256 * 1024;

  PreadFile() = default;
  explicit PreadFile(const std::filesystem::path &path, std::size_t capacity = defaultCapacity);

  PreadFile(const PreadFile &) = delete;
  PreadFile &operator=(const PreadFile &) = delete;
  PreadFile(PreadFile &&) noexcept = default;
  PreadFile &operator=(PreadFile &&) noexcept = default;

  // Opens path read-only. A failed open leaves any previous descriptor in place.
  bool open(const std::filesystem::path &path, std::size_t capacity = defaultCapacity);
  bool isOpen() const noexcept;

  // Bytes from offset 0, capped at the fixed buffer. An empty file is success.
  std::expected<std::string_view, std::errc> read();
  // First line of read(), without a trailing '\n'. Empty when the file is empty.
  std::expected<std::string_view, std::errc> readFirstLine();

  // Line starting at offset, which advances past the '\n'. A final line with no
  // newline is still returned. nullopt once offset is at the end of text.
  static std::optional<std::string_view> nextLine(std::string_view text, std::size_t &offset);

private:
  FdGuard fd;
  std::vector<char> buffer;
};
