#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "SharedState.hpp"
#include "UDSServer.hpp"

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {

// Spins up a real UDSServer on a private socket in a background thread and
// drives it through an actual client connection, exercising ProcessRequest
// end to end (parsing, dispatch, framing).
class UDSServerTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    path = "/tmp/hwmon_uds_test_" + std::to_string(::getpid()) + "_" +
           std::to_string(counter.fetch_add(1)) + ".sock";

    server.emplace(path, 10, state);
    runThread = std::jthread([this] { server->run(); });
    ASSERT_TRUE(waitForServer()) << "server did not start listening in time";
  }

  void TearDown() override {
    state.running.store(false);
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
    return fd;
  }

  bool waitForServer() {
    for (int attempt = 0; attempt < 100; ++attempt) {
      int fd = connectClient();
      if (fd >= 0) {
        ::close(fd);
        return true;
      }
      std::this_thread::sleep_for(20ms);
    }
    return false;
  }

  // Sends a single request line and returns the server's response line
  // (newline stripped). Returns std::nullopt on connection/IO failure.
  std::optional<std::string> request(const std::string &line) {
    int fd = connectClient();
    if (fd < 0) {
      return std::nullopt;
    }

    const std::string payload = line + "\n";
    if (::send(fd, payload.data(), payload.size(), 0) < 0) {
      ::close(fd);
      return std::nullopt;
    }

    std::string response;
    char buf[1024];
    while (response.find('\n') == std::string::npos) {
      ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
      if (n <= 0) {
        break;
      }
      response.append(buf, static_cast<size_t>(n));
    }
    ::close(fd);

    if (auto nl = response.find('\n'); nl != std::string::npos) {
      response.resize(nl);
    }
    return response;
  }

  SharedState state{1000};
  std::string path;
  std::optional<UDSServer> server;
  std::jthread runThread;
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

TEST_F(UDSServerTest, HandlesMultipleRequestsOnOneConnection) {
  int fd = connectClient();
  ASSERT_GE(fd, 0);

  const std::string payload = std::string(R"({"cmd":"ping"})") + "\n" +
                              R"({"cmd":"ping"})" + "\n";
  ASSERT_GT(::send(fd, payload.data(), payload.size(), 0), 0);

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
