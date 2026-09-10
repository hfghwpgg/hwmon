#include <argparse/argparse.hpp>
#include <atomic>
#include <csignal>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "CliParser.hpp"
#include "Runner.hpp"
#include "SharedState.hpp"
#include "UDSServer.hpp"
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
  Config config = CliParser(argc, argv);

  switch (config.debuglevel) {
  case 2:
    spdlog::set_level(spdlog::level::trace);
    break;
  case 1:
    spdlog::set_level(spdlog::level::debug);
    break;
  default:
    break;
  }

  if (config.refreshSocket) {
    unlink(config.sockPath.c_str());
    rmdir(config.sockPath.parent_path().c_str());
  }

  SharedState state{config.initialIntervalMs};

  gRunning = &state.running;
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  Runner runner{state, config.hwmonPath, true};
  runner.setup();
  dropPrivileges(config.dontDropRoot);

  UDSServer server{config.sockPath, config.backlog, state};

  std::jthread runnerThread{[&runner] { runner.run(); }};

  // Blocks on the accept loop until the shutdown flag is set.
  server.run();
  spdlog::debug("program ended gracefully");
  return 0;
}
