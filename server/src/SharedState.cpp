#include "SharedState.hpp"

#include <cstdint>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

SharedState::SharedState(unsigned int initialIntervalMs) :
    intervalMs(initialIntervalMs),
    wakeFd(::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {}

SharedState::~SharedState() {
  if (wakeFd >= 0) {
    ::close(wakeFd);
    wakeFd = -1;
  }
}

void SharedState::requestShutdown() noexcept {
  running.store(false, std::memory_order_relaxed);
  if (wakeFd >= 0) {
    // Never drained, so the descriptor stays readable for every waiter.
    const uint64_t one = 1;
    [[maybe_unused]] ssize_t written = ::write(wakeFd, &one, sizeof(one));
  }
}

bool SharedState::waitForShutdown(int timeoutMs) const noexcept {
  if (!running.load(std::memory_order_relaxed)) {
    return true;
  }
  if (wakeFd < 0) {
    return false;
  }

  pollfd pfd{.fd = wakeFd, .events = POLLIN, .revents = 0};
  const int ready = ::poll(&pfd, 1, timeoutMs);
  if (ready < 0) {
    // Interrupted or failed: fall back to the flag, which the handler sets first.
    return !running.load(std::memory_order_relaxed);
  }
  return ready > 0;
}
