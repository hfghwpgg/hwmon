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


  explicit SharedState(unsigned int initialIntervalMs) :
      intervalMs(initialIntervalMs) {}
};
