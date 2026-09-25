#pragma once

// Move-only RAII wrapper for a file descriptor. Guarantees the fd is
// closed exactly once on destruction, on every code path.
class FdGuard {
public:
  FdGuard() = default;
  explicit FdGuard(int fd) noexcept;
  ~FdGuard();

  FdGuard(FdGuard &&other) noexcept;
  FdGuard &operator=(FdGuard &&other) noexcept;

  FdGuard(const FdGuard &) = delete;
  FdGuard &operator=(const FdGuard &) = delete;

  int get() const noexcept {
    return fd;
  }
  bool valid() const noexcept {
    return fd >= 0;
  }

private:
  void reset() noexcept;
  int fd{-1};
};
