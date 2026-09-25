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
  // Ceilings, not reservations. The buffer starts small and shrinks to the
  // high-water mark after a read, so steady-state RSS tracks the bytes read.
  //
  // Hwmon attributes are a short number plus a newline (typically under 32
  // bytes). 512 leaves room for a long label without a page-sized buffer.
  static constexpr std::size_t defaultCapacity = 512;
  // Measured on a 4-CPU system: /proc/stat ~1.3 KiB (cpu lines ~40 B, plus a
  // ~1 KiB intr line), /proc/cpuinfo ~6.6 KiB (~1.7 KiB per logical CPU),
  // /proc/net/dev ~0.6 KiB (~120 B per interface). 32 KiB covers that with
  // headroom: on the order of 200 cpu lines, ~18 full cpuinfo blocks (the
  // model name is in the first block), and ~270 interfaces. Past this, read()
  // truncates and warns.
  static constexpr std::size_t largeCapacity = 32 * 1024;

  PreadFile() = default;
  explicit PreadFile(const std::filesystem::path &path, std::size_t capacity = defaultCapacity);

  PreadFile(const PreadFile &) = delete;
  PreadFile &operator=(const PreadFile &) = delete;
  PreadFile(PreadFile &&) noexcept = default;
  PreadFile &operator=(PreadFile &&) noexcept = default;

  // Opens path read-only. A failed open leaves any previous descriptor in place.
  bool open(const std::filesystem::path &path, std::size_t capacity = defaultCapacity);
  bool isOpen() const noexcept;

  // Bytes from offset 0, capped at the capacity passed to open(). An empty file
  // is success. Content past that ceiling is truncated.
  std::expected<std::string_view, std::errc> read();
  // First line of read(), without a trailing '\n'. Empty when the file is empty.
  std::expected<std::string_view, std::errc> readFirstLine();

  // Line starting at offset, which advances past the '\n'. A final line with no
  // newline is still returned. nullopt once offset is at the end of text.
  static std::optional<std::string_view> nextLine(std::string_view text, std::size_t &offset);

private:
  FdGuard fd;
  // Max bytes read() will keep. Zero until a successful open().
  std::size_t capacity_{0};
  // Largest successful read. The buffer is shrunk down to this.
  std::size_t highWater_{0};
  std::vector<char> buffer;
};
