#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../SharedState.hpp"
#include "../UDSServer.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

// Base fixture: owns a private directory per test and knows how to talk to a
// server over a real socket. It deliberately does not start anything, so
// tests can pick their own client limit or inject syscall failures.
class UDSServerFixture : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    root = fs::temp_directory_path() / ("hwmon_uds_test_" + std::to_string(::getpid()) + "_" +
                                        std::to_string(counter.fetch_add(1)));
    path = (root / "hwmon.sock").string();
  }

  void TearDown() override {
    stopServer();
    std::error_code ec;
    fs::remove_all(root, ec);
  }

  void startServer(size_t maxClients = 10, SocketOps ops = {}) {
    server.emplace(path, 10, maxClients, state, std::move(ops));
    runThread = std::jthread([this] { serverRunResult = server->run(); });
  }

  void stopServer() {
    state.requestShutdown();
    if (runThread.joinable()) {
      runThread.join();
    }
    server.reset();
  }

  int connectClient() {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
      return -1;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
      ::close(fd);
      return -1;
    }
    // Never let a broken server turn a failing assertion into a hung test.
    timeval timeout{.tv_sec = 5, .tv_usec = 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return fd;
  }

  // Connects, retrying while the server is still coming up.
  int connectClientWaiting() {
    for (int attempt = 0; attempt < 250; ++attempt) {
      int fd = connectClient();
      if (fd >= 0) {
        return fd;
      }
      std::this_thread::sleep_for(20ms);
    }
    return -1;
  }

  bool waitForServer() {
    int fd = connectClientWaiting();
    if (fd < 0) {
      return false;
    }
    ::close(fd);
    return true;
  }

  static bool sendAll(int fd, std::string_view payload) {
    size_t sent = 0;
    while (sent < payload.size()) {
      ssize_t n = ::send(fd, payload.data() + sent, payload.size() - sent, MSG_NOSIGNAL);
      if (n <= 0) {
        return false;
      }
      sent += static_cast<size_t>(n);
    }
    return true;
  }

  // Reads until a newline shows up, returning the line without it.
  // std::nullopt means the peer closed (or timed out) first.
  static std::optional<std::string> readLine(int fd) {
    std::string response;
    char buf[4096];
    while (response.find('\n') == std::string::npos) {
      ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
      if (n <= 0) {
        return std::nullopt;
      }
      response.append(buf, static_cast<size_t>(n));
    }
    response.resize(response.find('\n'));
    return response;
  }

  // Sends a single request line on a fresh connection and returns the reply.
  std::optional<std::string> request(const std::string &line) {
    int fd = connectClient();
    if (fd < 0) {
      return std::nullopt;
    }
    std::optional<std::string> response;
    if (sendAll(fd, line + "\n")) {
      response = readLine(fd);
    }
    ::close(fd);
    return response;
  }

  // A snapshot several times larger than the socket buffer, so the reply
  // cannot go out in a single send().
  static std::string makeLargeSnapshot() {
    json big = json::array();
    for (int i = 0; i < 40000; ++i) {
      big.push_back({{"name", "sensor" + std::to_string(i)}, {"value", i}});
    }
    return big.dump();
  }

  SharedState state{1000};
  fs::path root;
  std::string path;
  std::optional<UDSServer> server;
  std::jthread runThread;
  bool serverRunResult{false};
};

// Spins up a real UDSServer in a background thread and drives it through an
// actual client connection, exercising ProcessRequest end to end (parsing,
// dispatch, framing).
class UDSServerTest : public UDSServerFixture {
protected:
  void SetUp() override {
    UDSServerFixture::SetUp();
    startServer();
    ASSERT_TRUE(waitForServer()) << "server did not start listening in time";
  }
};

} // namespace

TEST_F(UDSServerTest, PingReturnsOk) {
  auto resp = request(R"({"cmd":"ping"})");
  ASSERT_TRUE(resp.has_value());
  const json j = json::parse(*resp);
  EXPECT_EQ(j["ok"], true);
}

TEST_F(UDSServerTest, SetIntervalUpdatesSharedState) {
  auto resp = request(R"({"cmd":"set_interval","value":2500})");
  ASSERT_TRUE(resp.has_value());
  const json j = json::parse(*resp);
  EXPECT_EQ(j["ok"], true);
  EXPECT_EQ(j["interval"].get<unsigned>(), 2500u);
  EXPECT_EQ(state.intervalMs.load(), 2500u);
}

TEST_F(UDSServerTest, SetIntervalRejectsZero) {
  auto resp = request(R"({"cmd":"set_interval","value":0})");
  ASSERT_TRUE(resp.has_value());
  const json j = json::parse(*resp);
  ASSERT_TRUE(j.contains("error"));
  EXPECT_EQ(state.intervalMs.load(), 1000u); // unchanged
}

TEST_F(UDSServerTest, SetIntervalRequiresValue) {
  auto resp = request(R"({"cmd":"set_interval"})");
  ASSERT_TRUE(resp.has_value());
  EXPECT_TRUE(json::parse(*resp).contains("error"));
}

TEST_F(UDSServerTest, GetWithoutSnapshotReturnsError) {
  auto resp = request(R"({"cmd":"get"})");
  ASSERT_TRUE(resp.has_value());
  EXPECT_TRUE(json::parse(*resp).contains("error"));
}

TEST_F(UDSServerTest, GetReturnsPublishedSnapshot) {
  const std::string snapshot = R"([{"name":"hwmon0"}])";
  state.snapshot.store(std::make_shared<const std::string>(snapshot));

  auto resp = request(R"({"cmd":"get"})");
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(*resp, snapshot);
  // The snapshot is forwarded verbatim and must remain valid, parseable JSON.
  const json parsed = json::parse(*resp);
  EXPECT_TRUE(parsed.is_array());
}

TEST_F(UDSServerTest, InvalidJsonIsRejected) {
  auto resp = request("this is not json");
  ASSERT_TRUE(resp.has_value());
  const json j = json::parse(*resp);
  EXPECT_EQ(j["error"], "invalid json");
}

TEST_F(UDSServerTest, MissingCmdIsRejected) {
  auto resp = request(R"({"value":1})");
  ASSERT_TRUE(resp.has_value());
  const json j = json::parse(*resp);
  EXPECT_EQ(j["error"], "missing cmd");
}

TEST_F(UDSServerTest, UnknownCommandIsReported) {
  auto resp = request(R"({"cmd":"frobnicate"})");
  ASSERT_TRUE(resp.has_value());
  const json j = json::parse(*resp);
  EXPECT_EQ(j["error"], "unknown command");
  EXPECT_EQ(j["cmd"], "frobnicate");
}

TEST_F(UDSServerTest, SetIntervalRejectsNegativeValue) {
  auto resp = request(R"({"cmd":"set_interval","value":-1})");
  ASSERT_TRUE(resp.has_value());
  EXPECT_TRUE(json::parse(*resp).contains("error"));
  EXPECT_EQ(state.intervalMs.load(), 1000u);
}

TEST_F(UDSServerTest, SetIntervalRejectsStringValue) {
  auto resp = request(R"({"cmd":"set_interval","value":"1000"})");
  ASSERT_TRUE(resp.has_value());
  EXPECT_TRUE(json::parse(*resp).contains("error"));
  EXPECT_EQ(state.intervalMs.load(), 1000u);
}

TEST_F(UDSServerTest, SetIntervalRejectsFloatValue) {
  auto resp = request(R"({"cmd":"set_interval","value":1.5})");
  ASSERT_TRUE(resp.has_value());
  EXPECT_TRUE(json::parse(*resp).contains("error"));
  EXPECT_EQ(state.intervalMs.load(), 1000u);
}

TEST_F(UDSServerTest, RejectsNonStringCmd) {
  auto respNum = request(R"({"cmd":123})");
  ASSERT_TRUE(respNum.has_value());
  EXPECT_EQ(json::parse(*respNum)["error"], "missing cmd");

  auto respNull = request(R"({"cmd":null})");
  ASSERT_TRUE(respNull.has_value());
  EXPECT_EQ(json::parse(*respNull)["error"], "missing cmd");
}

TEST_F(UDSServerTest, ResetCommandSetsResetFlag) {
  EXPECT_FALSE(state.resetFlag.load());
  auto resp = request(R"({"cmd":"reset"})");
  ASSERT_TRUE(resp.has_value());
  const json j = json::parse(*resp);
  EXPECT_EQ(j["ok"], true);
  EXPECT_TRUE(state.resetFlag.load());
}

TEST_F(UDSServerTest, HandlesMultipleRequestsOnOneConnection) {
  int fd = connectClient();
  ASSERT_GE(fd, 0);

  const std::string payload = std::string(R"({"cmd":"ping"})") + "\n" + R"({"cmd":"ping"})" + "\n";
  ASSERT_TRUE(sendAll(fd, payload));

  std::string response;
  int newlines = 0;
  char buf[1024];
  while (newlines < 2) {
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
      break;
    }
    response.append(buf, static_cast<size_t>(n));
    newlines = static_cast<int>(std::count(response.begin(), response.end(), '\n'));
  }
  ::close(fd);

  EXPECT_EQ(newlines, 2);
}

// ---------------------------------------------------------------------------
// Request framing
// ---------------------------------------------------------------------------

// A request is only complete once it is newline-terminated; anything else
// must sit in the buffer rather than being answered.
TEST_F(UDSServerTest, RequestWithoutNewlineIsNotAnswered) {
  int fd = connectClient();
  ASSERT_GE(fd, 0);
  ASSERT_TRUE(sendAll(fd, R"({"cmd":"ping"})"));

  // Half-close so the server sees EOF and drops us without ever replying.
  ::shutdown(fd, SHUT_WR);
  EXPECT_FALSE(readLine(fd).has_value());
  ::close(fd);

  // The server itself must be unaffected.
  auto resp = request(R"({"cmd":"ping"})");
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(json::parse(*resp)["ok"], true);
}

TEST_F(UDSServerTest, RequestSplitAcrossWritesIsAnsweredOnceComplete) {
  int fd = connectClient();
  ASSERT_GE(fd, 0);

  ASSERT_TRUE(sendAll(fd, R"({"cmd":)"));
  std::this_thread::sleep_for(50ms);
  ASSERT_TRUE(sendAll(fd, "\"ping\"}\n"));

  auto resp = readLine(fd);
  ::close(fd);
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(json::parse(*resp)["ok"], true);
}

TEST_F(UDSServerTest, RequestLargerThanLimitIsRejected) {
  int fd = connectClient();
  ASSERT_GE(fd, 0);

  // No newline anywhere, so the buffer just grows past the cap.
  const std::string junk(UDSServer::maxRequestBytes + 4096, 'a');
  sendAll(fd, junk); // may fail mid-way once the server hangs up, that is fine

  auto resp = readLine(fd);
  if (resp.has_value()) {
    EXPECT_EQ(json::parse(*resp)["error"], "request too large");
  }
  // Either way the connection must end up closed by the server.
  char buf[64];
  ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
  EXPECT_LE(n, 0);
  ::close(fd);

  auto ping = request(R"({"cmd":"ping"})");
  ASSERT_TRUE(ping.has_value());
  EXPECT_EQ(json::parse(*ping)["ok"], true);
}

TEST_F(UDSServerTest, RequestJustUnderLimitIsServed) {
  int fd = connectClient();
  ASSERT_GE(fd, 0);

  // Valid JSON padded with whitespace to just below the cap.
  std::string padded = R"({"cmd":"ping"})";
  padded.append(UDSServer::maxRequestBytes - padded.size() - 1, ' ');
  ASSERT_TRUE(sendAll(fd, padded + "\n"));

  auto resp = readLine(fd);
  ::close(fd);
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(json::parse(*resp)["ok"], true);
}

// ---------------------------------------------------------------------------
// Snapshots
// ---------------------------------------------------------------------------

// Multi-megabyte snapshots exceed the socket buffer, so the reply has to be
// written across several send() calls without losing or reordering bytes.
TEST_F(UDSServerTest, LargeSnapshotIsDeliveredIntact) {
  const std::string snapshot = makeLargeSnapshot();
  ASSERT_GT(snapshot.size(), 1u << 20);
  state.snapshot.store(std::make_shared<const std::string>(snapshot));

  auto resp = request(R"({"cmd":"get"})");
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(resp->size(), snapshot.size());
  EXPECT_EQ(*resp, snapshot);
}

TEST_F(UDSServerTest, LargeSnapshotSurvivesClientDisconnect) {
  state.snapshot.store(std::make_shared<const std::string>(makeLargeSnapshot()));

  // Ask for the snapshot and hang up immediately: the writing thread must
  // give up on the broken pipe instead of taking the server down.
  int fd = connectClient();
  ASSERT_GE(fd, 0);
  ASSERT_TRUE(sendAll(fd, std::string(R"({"cmd":"get"})") + "\n"));
  ::close(fd);

  auto ping = request(R"({"cmd":"ping"})");
  ASSERT_TRUE(ping.has_value());
  EXPECT_EQ(json::parse(*ping)["ok"], true);
}

// ---------------------------------------------------------------------------
// Concurrency and shutdown
// ---------------------------------------------------------------------------

TEST_F(UDSServerTest, ServesManyConcurrentClients) {
  constexpr int clientCount = 8; // below the fixture's limit of 10
  std::atomic<int> succeeded{0};
  std::vector<std::jthread> workers;
  workers.reserve(clientCount);

  for (int i = 0; i < clientCount; ++i) {
    workers.emplace_back([&] {
      int fd = connectClientWaiting();
      if (fd < 0) {
        return;
      }
      bool ok = true;
      for (int round = 0; round < 10 && ok; ++round) {
        ok = sendAll(fd, std::string(R"({"cmd":"ping"})") + "\n");
        if (ok) {
          auto line = readLine(fd);
          ok = line.has_value() && json::parse(*line)["ok"] == true;
        }
      }
      ::close(fd);
      if (ok) {
        succeeded.fetch_add(1);
      }
    });
  }
  workers.clear(); // joins

  EXPECT_EQ(succeeded.load(), clientCount);
}

TEST_F(UDSServerTest, ShutdownDisconnectsActiveClientsImmediately) {
  std::vector<int> fds;
  for (int i = 0; i < 4; ++i) {
    int fd = connectClient();
    ASSERT_GE(fd, 0);
    // Make sure the connection is really being served before we shut down.
    ASSERT_TRUE(sendAll(fd, std::string(R"({"cmd":"ping"})") + "\n"));
    ASSERT_TRUE(readLine(fd).has_value());
    fds.push_back(fd);
  }

  const auto start = std::chrono::steady_clock::now();
  state.requestShutdown();
  runThread.join();
  const auto elapsed = std::chrono::steady_clock::now() - start;

  // The poll timeout is 500 ms; waking on the shutdown eventfd must be
  // noticeably faster than waiting it out.
  EXPECT_LT(elapsed, 300ms);

  // Every client sees the connection go away rather than hanging.
  for (int fd : fds) {
    char buf[64];
    EXPECT_LE(::recv(fd, buf, sizeof(buf), 0), 0);
    ::close(fd);
  }
}

TEST_F(UDSServerTest, ShutdownWakesAnIdleServerImmediately) {
  const auto start = std::chrono::steady_clock::now();
  state.requestShutdown();
  runThread.join();
  EXPECT_LT(std::chrono::steady_clock::now() - start, 300ms);
}

// ---------------------------------------------------------------------------
// Client limit
// ---------------------------------------------------------------------------

TEST_F(UDSServerFixture, RejectsClientsOverTheLimit) {
  startServer(2);

  std::vector<int> accepted;
  for (int i = 0; i < 2; ++i) {
    int fd = connectClientWaiting();
    ASSERT_GE(fd, 0);
    // Round-tripping guarantees the slot is registered server side.
    ASSERT_TRUE(sendAll(fd, std::string(R"({"cmd":"ping"})") + "\n"));
    ASSERT_TRUE(readLine(fd).has_value());
    accepted.push_back(fd);
  }

  int extra = connectClientWaiting();
  ASSERT_GE(extra, 0);
  auto resp = readLine(extra);
  ASSERT_TRUE(resp.has_value()) << "over-limit client got no explanation";
  EXPECT_EQ(json::parse(*resp)["error"], "too many clients");

  char buf[64];
  EXPECT_LE(::recv(extra, buf, sizeof(buf), 0), 0) << "rejected client was not disconnected";
  ::close(extra);

  // The clients that made it in keep working.
  for (int fd : accepted) {
    EXPECT_TRUE(sendAll(fd, std::string(R"({"cmd":"ping"})") + "\n"));
    auto line = readLine(fd);
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(json::parse(*line)["ok"], true);
  }
  for (int fd : accepted) {
    ::close(fd);
  }
}

TEST_F(UDSServerFixture, FreedSlotIsReusedAfterClientDisconnects) {
  startServer(1);

  int first = connectClientWaiting();
  ASSERT_GE(first, 0);
  ASSERT_TRUE(sendAll(first, std::string(R"({"cmd":"ping"})") + "\n"));
  ASSERT_TRUE(readLine(first).has_value());
  ::close(first);

  // Once the slot is reaped a new client must be served normally again.
  for (int attempt = 0; attempt < 100; ++attempt) {
    int fd = connectClientWaiting();
    ASSERT_GE(fd, 0);
    ASSERT_TRUE(sendAll(fd, std::string(R"({"cmd":"ping"})") + "\n"));
    auto line = readLine(fd);
    ::close(fd);
    ASSERT_TRUE(line.has_value());
    const json j = json::parse(*line);
    if (j.contains("ok")) {
      SUCCEED();
      return;
    }
    std::this_thread::sleep_for(20ms);
  }
  FAIL() << "slot of a disconnected client was never freed";
}

// ---------------------------------------------------------------------------
// Syscall failures
// ---------------------------------------------------------------------------

TEST_F(UDSServerFixture, SocketFailureAbortsStartup) {
  SocketOps ops;
  ops.socket = [](int, int, int) {
    errno = EMFILE;
    return -1;
  };

  UDSServer failing{path, 10, 10, state, ops};
  EXPECT_FALSE(failing.run());
  EXPECT_FALSE(state.running.load());
}

TEST_F(UDSServerFixture, BindFailureAbortsStartup) {
  SocketOps ops;
  ops.bind = [](int, const sockaddr *, socklen_t) {
    errno = EADDRINUSE;
    return -1;
  };

  UDSServer failing{path, 10, 10, state, ops};
  EXPECT_FALSE(failing.run());
  EXPECT_FALSE(state.running.load());
  EXPECT_FALSE(fs::exists(path));
}

TEST_F(UDSServerFixture, ListenFailureAbortsStartup) {
  SocketOps ops;
  ops.listen = [](int, int) {
    errno = EOPNOTSUPP;
    return -1;
  };

  UDSServer failing{path, 10, 10, state, ops};
  EXPECT_FALSE(failing.run());
  EXPECT_FALSE(state.running.load());
}

// A real bind() failure, with no injection: /proc never lets us create the
// socket directory, so bind() fails with ENOENT.
TEST_F(UDSServerFixture, BindFailsWhenTheSocketDirectoryCannotBeCreated) {
  UDSServer failing{"/proc/hwmon_uds_test/hwmon.sock", 10, 10, state};
  EXPECT_FALSE(failing.run());
  EXPECT_FALSE(state.running.load());
}

// accept() can fail transiently (ECONNABORTED, EMFILE, ...); the loop must
// log it and carry on instead of dying.
TEST_F(UDSServerFixture, TransientAcceptFailureDoesNotKillTheServer) {
  auto failures = std::make_shared<std::atomic<int>>(0);
  SocketOps ops;
  ops.accept = [failures](int fd, sockaddr *addr, socklen_t *len) {
    if (failures->fetch_add(1) == 0) {
      errno = ECONNABORTED;
      return -1;
    }
    return ::accept(fd, addr, len);
  };

  startServer(10, ops);
  ASSERT_TRUE(waitForServer()) << "server did not recover from a failed accept";

  auto resp = request(R"({"cmd":"ping"})");
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(json::parse(*resp)["ok"], true);
  EXPECT_GE(failures->load(), 2);
}

// ---------------------------------------------------------------------------
// Socket path validation
// ---------------------------------------------------------------------------

TEST(UDSServerPathTest, AcceptsAPlainAbsolutePath) {
  EXPECT_FALSE(UDSServer::ValidateSocketPath("/tmp/hwmon/hwmon.sock").has_value());
}

TEST(UDSServerPathTest, RejectsEmptyPath) {
  EXPECT_TRUE(UDSServer::ValidateSocketPath("").has_value());
}

TEST(UDSServerPathTest, RejectsRelativePath) {
  EXPECT_TRUE(UDSServer::ValidateSocketPath("hwmon/hwmon.sock").has_value());
  EXPECT_TRUE(UDSServer::ValidateSocketPath("./hwmon.sock").has_value());
}

TEST(UDSServerPathTest, RejectsParentDirectoryTraversal) {
  EXPECT_TRUE(UDSServer::ValidateSocketPath("/tmp/hwmon/../../etc/hwmon.sock").has_value());
}

TEST(UDSServerPathTest, RejectsTrailingSeparator) {
  EXPECT_TRUE(UDSServer::ValidateSocketPath("/tmp/hwmon/").has_value());
}

// sun_path is 108 bytes; anything longer would be silently truncated.
TEST(UDSServerPathTest, RejectsOverlyLongPath) {
  const std::string longPath = "/tmp/" + std::string(200, 'a') + "/hwmon.sock";
  EXPECT_TRUE(UDSServer::ValidateSocketPath(longPath).has_value());
}

// The server creates and removes the parent directory, so it must own it.
TEST(UDSServerPathTest, RejectsPathDirectlyInRoot) {
  EXPECT_TRUE(UDSServer::ValidateSocketPath("/hwmon.sock").has_value());
}

TEST(UDSServerPathTest, RejectsSymlinkedParentDirectory) {
  const fs::path base = fs::temp_directory_path() /
                        ("hwmon_uds_symlink_" + std::to_string(::getpid()));
  const fs::path target = base / "real";
  const fs::path link = base / "link";
  fs::create_directories(target);
  std::error_code ec;
  fs::create_directory_symlink(target, link, ec);
  ASSERT_FALSE(ec) << ec.message();

  EXPECT_TRUE(UDSServer::ValidateSocketPath(link / "hwmon.sock").has_value());
  EXPECT_FALSE(UDSServer::ValidateSocketPath(target / "hwmon.sock").has_value());

  fs::remove_all(base, ec);
}

TEST(UDSServerPathTest, RejectsSymlinkedSocketPath) {
  const fs::path base = fs::temp_directory_path() /
                        ("hwmon_uds_symlink_file_" + std::to_string(::getpid()));
  fs::create_directories(base);
  const fs::path victim = base / "victim";
  std::ofstream{victim} << "precious";
  const fs::path link = base / "hwmon.sock";
  std::error_code ec;
  fs::create_symlink(victim, link, ec);
  ASSERT_FALSE(ec) << ec.message();

  EXPECT_TRUE(UDSServer::ValidateSocketPath(link).has_value());

  fs::remove_all(base, ec);
}

TEST(UDSServerPathTest, RejectsParentThatIsAFile) {
  const fs::path base = fs::temp_directory_path() /
                        ("hwmon_uds_fileparent_" + std::to_string(::getpid()));
  fs::create_directories(base);
  const fs::path file = base / "notadir";
  std::ofstream{file} << "x";

  EXPECT_TRUE(UDSServer::ValidateSocketPath(file / "hwmon.sock").has_value());

  std::error_code ec;
  fs::remove_all(base, ec);
}

TEST_F(UDSServerFixture, RefusesToStartOnAnUnsafePath) {
  UDSServer unsafeServer{"relative/hwmon.sock", 10, 10, state};
  EXPECT_FALSE(unsafeServer.run());
  EXPECT_FALSE(state.running.load());
}

// An existing regular file at the socket path must never be bound over or
// deleted, even when the server shuts down afterwards.
TEST_F(UDSServerFixture, RefusesToBindOverAnExistingFileAndKeepsIt) {
  fs::create_directories(root);
  std::ofstream{path} << "important";

  {
    UDSServer blocked{path, 10, 10, state};
    EXPECT_FALSE(blocked.run());
  }

  ASSERT_TRUE(fs::is_regular_file(path));
  std::ifstream in{path};
  std::string contents;
  in >> contents;
  EXPECT_EQ(contents, "important");
}
