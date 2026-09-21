#include "UDSServer.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <poll.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

#include "SharedState.hpp"
#include "helpers.hpp"

using json = nlohmann::json;

namespace {
// How long a poll() may block before we re-check the loop conditions. The
// shutdown eventfd is always part of the poll set, so this is only a
// backstop for callers that flip SharedState::running by hand.
constexpr int pollTimeoutMs = 500;
} // namespace

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
UDSServer::UDSServer(std::filesystem::path udsPath, int backlog, size_t maxClients,
                     SharedState &state, SocketOps ops) :
    udsPath(std::move(udsPath)),
    backlog(backlog),
    maxClients(maxClients),
    state(state),
    ops(std::move(ops)) {}

UDSServer::~UDSServer() {
  StopAllClients();
  listenFd = FdGuard{};
  // Only clean up files we actually created; a failed setup must not delete
  // whatever happens to live at the configured path.
  if (bound) {
    ::unlink(udsPath.c_str());
    ::rmdir(udsPath.parent_path().c_str());
  }
}

std::optional<std::string> UDSServer::ValidateSocketPath(const std::filesystem::path &path) {
  if (path.empty()) {
    return "path is empty";
  }
  if (!path.is_absolute()) {
    return "path must be absolute";
  }
  if (path.filename().empty()) {
    return "path must not end with a separator";
  }

  for (const auto &component : path) {
    if (component == "..") {
      return "path must not contain '..'";
    }
  }

  sockaddr_un addr{};
  if (path.string().size() >= sizeof(addr.sun_path)) {
    return "path is longer than " + std::to_string(sizeof(addr.sun_path) - 1) + " characters";
  }

  const std::filesystem::path parent = path.parent_path();
  if (parent == path.root_path()) {
    // We create and remove the parent directory, so it has to be our own.
    return "path must live in a dedicated directory, not directly in " + parent.string();
  }

  std::error_code ec;
  if (std::filesystem::is_symlink(parent, ec)) {
    return "parent directory is a symlink";
  }
  if (std::filesystem::exists(parent, ec) && !std::filesystem::is_directory(parent, ec)) {
    return "parent path exists but is not a directory";
  }
  if (std::filesystem::is_symlink(path, ec)) {
    return "path is a symlink";
  }

  return std::nullopt;
}

bool UDSServer::setup() {
  if (const auto problem = ValidateSocketPath(udsPath)) {
    spdlog::error("refusing unsafe socket path {}: {}", udsPath.string(), *problem);
    return false;
  }

  if (std::filesystem::exists(udsPath)) {
    spdlog::error("socket already exists, exiting...");
    spdlog::info("it probably means that that other instance is running in the background");
    spdlog::info("or that you pointed socket at a regular file");
    spdlog::info("run server with --refresh-socket to remove it");
    return false;
  }

  FdGuard fd{ops.socket(AF_UNIX, SOCK_STREAM, 0)};
  if (!fd.valid()) {
    spdlog::error("couldn't open socket: {}", std::strerror(errno));
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::memcpy(addr.sun_path, udsPath.c_str(), udsPath.string().size() + 1);

  // Create socket folder with correct privileges
  ::mkdir(udsPath.parent_path().c_str(), 0700);

  if (ops.bind(fd.get(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    spdlog::error("couldn't bind socket: {}", std::strerror(errno));
    return false;
  }
  bound = true;

  if (ops.listen(fd.get(), backlog) != 0) {
    spdlog::error("listen failed: {}", std::strerror(errno));
    return false;
  }

  listenFd = std::move(fd);
  return true;
}

bool UDSServer::run() {
  if (!setup()) {
    state.requestShutdown();
    return false;
  }

  spdlog::info("UDS server listening on {} (max {} concurrent clients)", udsPath.string(),
               maxClients);

  while (state.running.load(std::memory_order_relaxed)) {
    std::array<pollfd, 2> pfds{
        pollfd{.fd = listenFd.get(), .events = POLLIN, .revents = 0},
        pollfd{.fd = state.shutdownFd(), .events = POLLIN, .revents = 0},
    };

    const int ready = ::poll(pfds.data(), pfds.size(), pollTimeoutMs);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      spdlog::error("poll failed: {}", std::strerror(errno));
      break;
    }
    if (pfds[1].revents & POLLIN) {
      break; // shutdown requested
    }
    if (ready == 0) {
      ReapFinishedClients();
      continue;
    }

    if (pfds[0].revents & POLLIN) {
      FdGuard clientFd{ops.accept(listenFd.get(), nullptr, nullptr)};
      if (!clientFd.valid()) {
        if (errno == EINTR) {
          continue;
        }
        spdlog::error("accept failed: {}", std::strerror(errno));
        continue;
      }

      // Free up slots of clients that already hung up before judging the limit.
      ReapFinishedClients();
      if (clients.size() >= maxClients) {
        spdlog::warn("client limit of {} reached, rejecting connection", maxClients);
        RejectClient(clientFd, "too many clients");
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

  StopAllClients();
  return true;
}

void UDSServer::ReapFinishedClients() {
  std::erase_if(clients, [](const ClientSlot &client) {
    return client.done->load(std::memory_order_acquire);
  });
}

void UDSServer::StopAllClients() {
  // Signal and join all client threads before tearing down.
  for (auto &client : clients) {
    client.thread.request_stop();
  }
  clients.clear();
}

void UDSServer::RejectClient(const FdGuard &clientFd, std::string_view reason) {
  const std::string response = json{{"error", reason}}.dump() + "\n";
  // Never block the accept loop on a client that isn't reading.
  [[maybe_unused]] ssize_t sent =
      ::send(clientFd.get(), response.data(), response.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
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

  std::string buffer;
  std::array<char, 4096> chunk{};

  while (!stopToken.stop_requested() && state.running.load(std::memory_order_relaxed)) {
    std::array<pollfd, 2> pfds{
        pollfd{.fd = clientFd.get(), .events = POLLIN, .revents = 0},
        pollfd{.fd = state.shutdownFd(), .events = POLLIN, .revents = 0},
    };

    const int ready = ::poll(pfds.data(), pfds.size(), pollTimeoutMs);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (pfds[1].revents & POLLIN) {
      break; // shutdown requested
    }
    if (ready == 0) {
      continue; // timeout: re-check stop conditions
    }
    if (pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
      break;
    }

    ssize_t received = ::recv(clientFd.get(), chunk.data(), chunk.size(), 0);
    if (received <= 0) {
      break; // peer closed or error
    }

    buffer.append(chunk.data(), static_cast<size_t>(received));
    if (buffer.size() > maxRequestBytes) {
      spdlog::warn("client request exceeded limit of {} bytes, dropping connection",
                   maxRequestBytes);
      RejectClient(clientFd, "request too large");
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
          if (n < 0 && errno == EINTR) {
            continue;
          }
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
    if (value < 50) {
      return json{{"error", "interval must be >= 50"}}.dump();
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
