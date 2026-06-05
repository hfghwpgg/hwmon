#include "Runner.hpp"
#include "SharedState.hpp"
#include "UDSServer.hpp"
#include <atomic>
#include <csignal>
#include <print>
#include <string>
#include <thread>

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
  const std::string path = "/tmp/hwmon.sock";
  constexpr unsigned int initialIntervalMs = 1000;
  constexpr int backlog = 10;

  SharedState state{initialIntervalMs};

  gRunning = &state.running;
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  Runner runner{state};
  UDSServer server{path, backlog, state};

  std::jthread runnerThread{[&runner] { runner.Run(); }};

  // Blocks on the accept loop until the shutdown flag is set.
  server.Run();

  std::print("program finished\n");
  return 0;
}
