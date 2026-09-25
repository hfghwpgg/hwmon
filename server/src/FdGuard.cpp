#include "FdGuard.hpp"

#include <unistd.h>
#include <utility>

FdGuard::FdGuard(int fd) noexcept :
    fd(fd) {}

FdGuard::~FdGuard() {
  reset();
}

FdGuard::FdGuard(FdGuard &&other) noexcept :
    fd(std::exchange(other.fd, -1)) {}

FdGuard &FdGuard::operator=(FdGuard &&other) noexcept {
  if (this != &other) {
    reset();
    fd = std::exchange(other.fd, -1);
  }
  return *this;
}

void FdGuard::reset() noexcept {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}
