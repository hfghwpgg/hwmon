#include <atomic>
#include <csignal>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>

#include "Runner.hpp"
#include "SharedState.hpp"
#include "UDSServer.hpp"

namespace {
std::atomic<bool> *gRunning = nullptr;

void HandleSignal(int sig) {
  (void)sig;
  if (gRunning != nullptr) {
    gRunning->store(false, std::memory_order_relaxed);
  }
}
} // namespace

int main() {
#ifdef DEBUG
  spdlog::set_level(spdlog::level::debug);
#endif
  const std::string path = "/tmp/hwmon.sock";
  constexpr unsigned int initialIntervalMs = 1000;
  constexpr int backlog = 10;

  SharedState state{initialIntervalMs};

  gRunning = &state.running;
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  Runner runner{state};
  UDSServer server{path, backlog, state};

  std::jthread runnerThread{[&runner] { runner.run(); }};

  // Blocks on the accept loop until the shutdown flag is set.
  server.run();
  spdlog::info("program ended gracefully");
  return 0;
}
