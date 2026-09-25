#pragma once
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <vector>

#include "FdGuard.hpp"
#include "SharedState.hpp"
#include "helpers.hpp"

struct SharedState;

// Indirection over the socket syscalls so tests can inject failures for
// paths that are otherwise impossible to reach (a failing listen(), an
// accept() that reports a transient error, ...).
struct SocketOps {
  std::function<int(int, int, int)> socket = [](int domain, int type, int protocol) {
    return ::socket(domain, type, protocol);
  };
  std::function<int(int, const sockaddr *, socklen_t)> bind =
      [](int fd, const sockaddr *addr, socklen_t len) { return ::bind(fd, addr, len); };
  std::function<int(int, int)> listen = [](int fd, int backlog) { return ::listen(fd, backlog); };
  std::function<int(int, sockaddr *, socklen_t *)> accept =
      [](int fd, sockaddr *addr, socklen_t *len) { return ::accept(fd, addr, len); };
};

// Pull-based Unix domain socket server. Serves the latest sensor JSON
// snapshot and accepts control commands. One jthread per client.
class UDSServer {
public:
  // Largest request a single client may buffer before it gets disconnected.
  static constexpr size_t maxRequestBytes = 64 * 1024;

  UDSServer(std::filesystem::path udsPath, int backlog, size_t maxClients, SharedState &state,
            SocketOps ops = {});
  ~UDSServer();

  UDSServer(const UDSServer &) = delete;
  UDSServer &operator=(const UDSServer &) = delete;

  // Runs the accept loop until shutdown is requested. False means the
  // server never got to listen (setup failed).
  bool run();

  // Rejects socket paths we refuse to create or delete files at. Returns the
  // reason when the path is unsafe, std::nullopt when it is fine.
  static std::optional<std::string> ValidateSocketPath(const std::filesystem::path &path);

private:
  // A running client connection plus a flag it sets when it finishes,
  // letting the accept loop reap (join) completed threads.
  struct ClientSlot {
    std::shared_ptr<std::atomic<bool>> done;
    std::jthread thread;
  };

  bool setup();
  void HandleClient(std::stop_token stopToken, FdGuard clientFd,
                    std::shared_ptr<std::atomic<bool>> done);
  std::string ProcessRequest(std::string_view request);
  void ReapFinishedClients();
  void StopAllClients();
  // Best-effort "go away" line for a connection we are not going to serve.
  static void RejectClient(const FdGuard &clientFd, std::string_view reason);

  const std::filesystem::path udsPath;
  const int backlog;
  const size_t maxClients;
  SharedState &state;
  SocketOps ops;

  // Set once bind() succeeded, so we only ever unlink a socket we created.
  bool bound{false};
  FdGuard listenFd;
  std::vector<ClientSlot> clients;
};
