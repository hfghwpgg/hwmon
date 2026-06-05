#pragma once
#include "SharedState.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

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

  int get() const noexcept { return fd; }
  bool valid() const noexcept { return fd >= 0; }

private:
  void reset() noexcept;
  int fd{-1};
};

// Pull-based Unix domain socket server. Serves the latest sensor JSON
// snapshot and accepts control commands. One jthread per client.
class UDSServer {
public:
  UDSServer(std::string udsPath, int backlog, SharedState &state);
  ~UDSServer();

  UDSServer(const UDSServer &) = delete;
  UDSServer &operator=(const UDSServer &) = delete;

  void Run();

private:
  // A running client connection plus a flag it sets when it finishes,
  // letting the accept loop reap (join) completed threads.
  struct ClientSlot {
    std::shared_ptr<std::atomic<bool>> done;
    std::jthread thread;
  };

  bool Setup();
  void HandleClient(std::stop_token stopToken, FdGuard clientFd,
                    std::shared_ptr<std::atomic<bool>> done);
  std::string ProcessRequest(std::string_view request);
  void ReapFinishedClients();

  const std::string udsPath;
  const int backlog;
  SharedState &state;

  FdGuard listenFd;
  std::vector<ClientSlot> clients;
};
