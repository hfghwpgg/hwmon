#include "UDSServer.hpp"

#include <nlohmann/detail/json_ref.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <fmt/base.h>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "SharedState.hpp"

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// FdGuard
// ---------------------------------------------------------------------------
FdGuard::FdGuard(int fd) noexcept :
    fd(fd) {}

FdGuard::~FdGuard() {
  reset();
}

FdGuard::FdGuard(FdGuard &&other) noexcept :
    fd(std::exchange(other.fd, -1)) {}

FdGuard &FdGuard::operator=(FdGuard &&other) noexcept {
  if (this != &other) {
    reset();
    fd = std::exchange(other.fd, -1);
  }
  return *this;
}

void FdGuard::reset() noexcept {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}

// ---------------------------------------------------------------------------
// UDSServer
// ---------------------------------------------------------------------------
UDSServer::UDSServer(std::string udsPath, int backlog, SharedState &state) :
    udsPath(std::move(udsPath)),
    backlog(backlog),
    state(state) {}

UDSServer::~UDSServer() {
  // Ask any in-flight client threads to stop; jthreads join on destruction.
  for (auto &client : clients) {
    client.thread.request_stop();
  }
  clients.clear();
  listenFd = FdGuard{};
  ::unlink(udsPath.c_str());
}

bool UDSServer::setup() {
  FdGuard fd{::socket(AF_UNIX, SOCK_STREAM, 0)};
  if (!fd.valid()) {
    spdlog::error("ERROR: couldn't open socket: {}", std::strerror(errno));
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (udsPath.size() >= sizeof(addr.sun_path)) {
    spdlog::error("ERROR: socket path too long: {}", udsPath);
    return false;
  }
  std::memcpy(addr.sun_path, udsPath.c_str(), udsPath.size() + 1);

  // Remove a stale socket file from a previous run before binding.
  ::unlink(udsPath.c_str());

  if (::bind(fd.get(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    spdlog::error("ERROR: couldn't bind socket: {}", std::strerror(errno));
    return false;
  }

  if (::listen(fd.get(), backlog) != 0) {
    spdlog::error("ERROR: listen failed: {}", std::strerror(errno));
    return false;
  }

  listenFd = std::move(fd);
  return true;
}

void UDSServer::run() {
  if (!setup()) {
    state.running.store(false);
    return;
  }

  spdlog::info("UDS server listening on {}", udsPath);

  while (state.running.load(std::memory_order_relaxed)) {
    pollfd pfd{.fd = listenFd.get(), .events = POLLIN, .revents = 0};

    // Short timeout so we periodically observe the shutdown flag.
    int ready = ::poll(&pfd, 1, 500);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      spdlog::error("ERROR: poll failed: {}", std::strerror(errno));
      break;
    }
    if (ready == 0) {
      ReapFinishedClients();
      continue;
    }

    if (pfd.revents & POLLIN) {
      FdGuard clientFd{::accept(listenFd.get(), nullptr, nullptr)};
      if (!clientFd.valid()) {
        if (errno == EINTR) {
          continue;
        }
        spdlog::error("ERROR: accept failed: {}", std::strerror(errno));
        continue;
      }

      auto done = std::make_shared<std::atomic<bool>>(false);
      clients.push_back(ClientSlot{
          done,
          std::jthread([this, fd = std::move(clientFd), done](std::stop_token stopToken) mutable {
            HandleClient(std::move(stopToken), std::move(fd), done);
          }),
      });
    }

    ReapFinishedClients();
  }

  // Signal and join all client threads before tearing down.
  for (auto &client : clients) {
    client.thread.request_stop();
  }
  clients.clear();
}

void UDSServer::ReapFinishedClients() {
  std::erase_if(clients, [](const ClientSlot &client) {
    return client.done->load(std::memory_order_acquire);
  });
}

void UDSServer::HandleClient(std::stop_token stopToken, FdGuard clientFd,
                             std::shared_ptr<std::atomic<bool>> done) {
  // Mark this slot reapable no matter how we exit.
  struct DoneGuard {
    std::shared_ptr<std::atomic<bool>> flag;
    ~DoneGuard() {
      flag->store(true, std::memory_order_release);
    }
  } doneGuard{done};

  constexpr size_t maxRequestBytes = 64 * 1024;
  std::string buffer;
  std::array<char, 4096> chunk{};

  while (!stopToken.stop_requested() && state.running.load(std::memory_order_relaxed)) {
    pollfd pfd{.fd = clientFd.get(), .events = POLLIN, .revents = 0};
    int ready = ::poll(&pfd, 1, 500);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (ready == 0) {
      continue; // timeout: re-check stop conditions
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      break;
    }

    ssize_t received = ::recv(clientFd.get(), chunk.data(), chunk.size(), 0);
    if (received <= 0) {
      break; // peer closed or error
    }

    buffer.append(chunk.data(), static_cast<size_t>(received));
    if (buffer.size() > maxRequestBytes) {
      spdlog::warn("WARN: client request exceeded limit, dropping connection");
      break;
    }

    // Process every complete, newline-delimited request in the buffer.
    size_t newline;
    while ((newline = buffer.find('\n')) != std::string::npos) {
      std::string_view line{buffer.data(), newline};
      std::string response = ProcessRequest(line);
      response.push_back('\n');

      size_t sent = 0;
      bool sendFailed = false;
      while (sent < response.size()) {
        ssize_t n =
            ::send(clientFd.get(), response.data() + sent, response.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) {
          sendFailed = true;
          break;
        }
        sent += static_cast<size_t>(n);
      }
      if (sendFailed) {
        return;
      }

      buffer.erase(0, newline + 1);
    }
  }
}

std::string UDSServer::ProcessRequest(std::string_view request) {
  json req;
  try {
    req = json::parse(request);
  } catch (const json::exception &e) {
    return json{{"error", "invalid json"}}.dump();
  }

  if (!req.is_object() || !req.contains("cmd") || !req["cmd"].is_string()) {
    return json{{"error", "missing cmd"}}.dump();
  }

  const std::string cmd = req["cmd"].get<std::string>();

  if (cmd == "get") {
    auto snapshot = state.snapshot.load(std::memory_order_acquire);
    if (!snapshot) {
      return json{{"error", "no data yet"}}.dump();
    }
    return *snapshot;
  }

  if (cmd == "set_interval") {
    if (!req.contains("value") || !req["value"].is_number_unsigned()) {
      return json{{"error", "set_interval requires unsigned 'value'"}}.dump();
    }
    unsigned int value = req["value"].get<unsigned int>();
    if (value == 0) {
      return json{{"error", "interval must be > 0"}}.dump();
    }
    state.intervalMs.store(value, std::memory_order_relaxed);
    return json{{"ok", true}, {"interval", value}}.dump();
  }

  if (cmd == "reset") {
    state.resetFlag.store(true, std::memory_order_relaxed);
    return json{{"ok", true}}.dump();
  }

  if (cmd == "ping") {
    return json{{"ok", true}}.dump();
  }

  return json{{"error", "unknown command"}, {"cmd", cmd}}.dump();
}
