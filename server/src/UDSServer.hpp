#pragma once
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <vector>

#include "SharedState.hpp"
#include "helpers.hpp"

struct SharedState;

// Move-only RAII wrapper for a file descriptor. Guarantees the fd is
// closed exactly once on destruction, on every code path.
class FdGuard {
public:
  FdGuard() = default;
  explicit FdGuard(int fd) noexcept;
  ~FdGuard();

  FdGuard(FdGuard &&other) noexcept;
  FdGuard &operator=(FdGuard &&other) noexcept;

  FdGuard(const FdGuard &) = delete;
  FdGuard &operator=(const FdGuard &) = delete;

  int get() const noexcept {
    return fd;
  }
  bool valid() const noexcept {
    return fd >= 0;
  }

private:
  void reset() noexcept;
  int fd{-1};
};

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

// Abstract-namespace address, displayed as @hwmon. The leading NUL is added
// by initAbstractAddress(); this is the name clients pass after that NUL.
inline constexpr std::string_view abstractSocketName = "hwmon";

// Fills an abstract-namespace sockaddr. `name` is the bytes after the leading
// NUL. Returns the address length to pass to bind()/connect(), or 0 when the
// name is empty or does not fit in sun_path.
socklen_t initAbstractAddress(sockaddr_un &addr, std::string_view name);

// Pull-based Unix domain socket server. Serves the latest sensor JSON
// snapshot and accepts control commands. One jthread per client.
// Listens in the abstract namespace, so the socket is not a filesystem path.
class UDSServer {
public:
  // Largest request a single client may buffer before it gets disconnected.
  static constexpr size_t maxRequestBytes = 64 * 1024;

  UDSServer(std::string socketName, int backlog, size_t maxClients, SharedState &state,
            SocketOps ops = {});
  ~UDSServer();

  UDSServer(const UDSServer &) = delete;
  UDSServer &operator=(const UDSServer &) = delete;

  // Runs the accept loop until shutdown is requested. False means the
  // server never got to listen (setup failed).
  bool run();

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

  const std::string socketName;
  const int backlog;
  const size_t maxClients;
  SharedState &state;
  SocketOps ops;

  FdGuard listenFd;
  std::vector<ClientSlot> clients;
};
