#include <csignal>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <thread>

#include "CliParser.hpp"
#include "Runner.hpp"
#include "SharedState.hpp"
#include "UDSServer.hpp"
#include "dropPrivileges.hpp"

namespace {
SharedState *gState = nullptr;

void HandleSignal(int sig) {
  (void)sig;
  if (gState != nullptr) {
    // Wakes the accept loop and the runner immediately instead of letting
    // them notice the flag at the end of the current poll/interval.
    gState->requestShutdown();
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

  SharedState state{config.initialIntervalMs};

  gState = &state;
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  bool served = false;
  // this block is only for a correct order
  // of debug messsages
  {
    Runner runner{state, config.hwmonPath, true};
    runner.setup();

    if (!config.dontDropRoot) {
      dropPrivileges();
    }

    UDSServer server{std::string{abstractSocketName}, config.backlog, config.maxClients, state};
    std::jthread runnerThread{[&runner] { runner.run(); }};

    // Blocks on the accept loop until the shutdown flag is set.
    served = server.run();
  }

  if (!served) {
    spdlog::error("server failed to start");
    return 1;
  }

  spdlog::info("program ended gracefully");
  return 0;
}
