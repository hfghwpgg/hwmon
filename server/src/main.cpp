#include <argparse/argparse.hpp>
#include <cerrno>
#include <csignal>
#include <cstring>
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
#include "helpers.hpp"

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

std::string describeFileType(mode_t mode) {
  if (S_ISREG(mode)) {
    return "a regular file";
  }
  if (S_ISDIR(mode)) {
    return "a directory";
  }
  if (S_ISLNK(mode)) {
    return "a symlink";
  }
  if (S_ISFIFO(mode)) {
    return "a fifo";
  }
  if (S_ISCHR(mode) || S_ISBLK(mode)) {
    return "a device node";
  }
  return "of an unknown type";
}

// --refresh-socket deletes whatever sits at the socket path, so make sure it
// really is a leftover socket before removing anything.
bool removeStaleSocket(const helpers::fs::path &path) {
  if (const auto problem = UDSServer::ValidateSocketPath(path)) {
    spdlog::error("refusing to remove unsafe socket path {}: {}", path.string(), *problem);
    return false;
  }

  struct stat info{};
  if (::lstat(path.c_str(), &info) != 0) {
    if (errno == ENOENT) {
      spdlog::debug("nothing to remove at {}", path.string());
      ::rmdir(path.parent_path().c_str());
      return true;
    }
    spdlog::error("couldn't stat {}: {}", path.string(), std::strerror(errno));
    return false;
  }

  if (!S_ISSOCK(info.st_mode)) {
    spdlog::error("{} is {}, not a socket - refusing to remove it", path.string(),
                  describeFileType(info.st_mode));
    return false;
  }

  if (::unlink(path.c_str()) != 0) {
    spdlog::error("couldn't remove {}: {}", path.string(), std::strerror(errno));
    return false;
  }
  ::rmdir(path.parent_path().c_str());
  spdlog::info("removed stale socket {}", path.string());
  return true;
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

  if (config.refreshSocket && !removeStaleSocket(config.sockPath)) {
    return 1;
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

    UDSServer server{config.sockPath, config.backlog, config.maxClients, state};
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
