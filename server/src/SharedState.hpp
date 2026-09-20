#pragma once
#include <atomic>
#include <memory>
#include <string>

// State shared between the background Runner (producer) and the
// UDSServer (consumer). All access is lock-free via atomics.
struct SharedState {
  // Sensor refresh interval in milliseconds, changeable at runtime.
  std::atomic<unsigned int> intervalMs;

  // Latest serialized JSON snapshot. Readers load() a shared_ptr that
  // stays valid even while the producer swaps in a newer snapshot.
  std::atomic<std::shared_ptr<const std::string>> snapshot;

  // Cooperative shutdown flag (also set from the signal handler).
  std::atomic<bool> running{true};

  std::atomic<bool> resetFlag{false};

  explicit SharedState(unsigned int initialIntervalMs);
  ~SharedState();

  SharedState(const SharedState &) = delete;
  SharedState &operator=(const SharedState &) = delete;

  // Clears `running` and wakes everything blocked in waitForShutdown() or
  // polling shutdownFd(). Async-signal-safe, so a signal handler may call it.
  void requestShutdown() noexcept;

  // Sleeps up to timeoutMs (negative waits forever), returning early as soon
  // as shutdown is requested. True means shutdown, false means timed out.
  bool waitForShutdown(int timeoutMs) const noexcept;

  // Level-triggered eventfd that becomes readable on shutdown and stays
  // readable. Add it to a poll() set to make any wait interruptible.
  int shutdownFd() const noexcept {
    return wakeFd;
  }

private:
  int wakeFd{-1};
};
