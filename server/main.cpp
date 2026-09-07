#include <argparse/argparse.hpp>
#include <atomic>
#include <csignal>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "Runner.hpp"
#include "SharedState.hpp"
#include "UDSServer.hpp"
#include "configManager.hpp"
#include "dropPrivileges.hpp"

namespace {
std::atomic<bool> *gRunning = nullptr;

void HandleSignal(int sig) {
  (void)sig;
  if (gRunning != nullptr) {
    gRunning->store(false, std::memory_order_relaxed);
  }
}
} // namespace

int main(int argc, char *argv[]) {
  Config config = configManager(argc, argv);
  if (config.debug) {
    spdlog::set_level(spdlog::level::debug);
  }

  SharedState state{config.initialIntervalMs};

  gRunning = &state.running;
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  Runner runner{state, config.hwmonPath, true};
  runner.setup();
  dropPrivileges();

  UDSServer server{config.sockFolder, config.sockPath, config.backlog, state};

  std::jthread runnerThread{[&runner] { runner.run(); }};

  // Blocks on the accept loop until the shutdown flag is set.
  server.run();
  spdlog::info("program ended gracefully");
  return 0;
}
