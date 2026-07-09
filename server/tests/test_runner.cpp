#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <unistd.h>

#include "Runner.hpp"
#include "SharedState.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {

class RunnerResetTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    root = fs::temp_directory_path() / ("runner_test_" + std::to_string(::getpid()) + "_" +
                                       std::to_string(counter.fetch_add(1)));
  }

  void TearDown() override {
    state.running.store(false);
    if (runThread.joinable()) {
      runThread.join();
    }
    std::error_code ec;
    fs::remove_all(root, ec);
  }

  void writeFile(const fs::path &path, const std::string &contents) {
    std::ofstream f{path, std::ios::trunc};
    f << contents;
  }

  void createFakeHwmon() {
    const fs::path hwmonRoot = root / "hwmon";
    const fs::path chip = hwmonRoot / "hwmon0";
    fs::create_directories(chip);
    writeFile(chip / "name", "testchip");
    writeFile(chip / "temp1_input", "30000");
    hwmonPath = hwmonRoot;
  }

  static bool snapshotHasSensorTimes(const std::string &snapshot, std::size_t minTimes) {
    const json devices = json::parse(snapshot);
    if (!devices.is_array()) {
      return false;
    }
    for (const auto &device : devices) {
      if (!device.contains("sensors")) {
        continue;
      }
      for (const auto &sensor : device["sensors"]) {
        if (sensor["readings"]["times"].get<std::size_t>() >= minTimes) {
          return true;
        }
      }
    }
    return false;
  }

  static bool allSensorTimesAre(const std::string &snapshot, std::size_t times) {
    const json devices = json::parse(snapshot);
    if (!devices.is_array() || devices.empty()) {
      return false;
    }
    bool foundSensor = false;
    for (const auto &device : devices) {
      if (!device.contains("sensors")) {
        continue;
      }
      for (const auto &sensor : device["sensors"]) {
        foundSensor = true;
        if (sensor["readings"]["times"].get<std::size_t>() != times) {
          return false;
        }
      }
    }
    return foundSensor;
  }

  bool waitForSnapshot(const std::function<bool(const std::string &)> &predicate,
                       std::chrono::milliseconds timeout = 3s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      if (auto snap = state.snapshot.load()) {
        if (predicate(*snap)) {
          return true;
        }
      }
      std::this_thread::sleep_for(20ms);
    }
    return false;
  }

  void startRunner() {
    runner.emplace(state, hwmonPath, false);
    runner->setup();
    runThread = std::jthread([this] { runner->run(); });
  }

  SharedState state{50};
  fs::path root;
  fs::path hwmonPath;
  std::optional<Runner> runner;
  std::jthread runThread;
};

} // namespace

TEST_F(RunnerResetTest, ResetFlagClearsSensorTimesInSnapshot) {
  createFakeHwmon();
  startRunner();

  ASSERT_TRUE(waitForSnapshot([](const std::string &s) { return snapshotHasSensorTimes(s, 2); }));

  state.resetFlag.store(true, std::memory_order_relaxed);
  // reset clears aggregates, then the same loop iteration reads once more.
  ASSERT_TRUE(waitForSnapshot([](const std::string &s) { return allSensorTimesAre(s, 1); }));
}

TEST_F(RunnerResetTest, ResetFlagIsClearedAfterProcessing) {
  createFakeHwmon();
  startRunner();

  ASSERT_TRUE(waitForSnapshot([](const std::string &s) { return snapshotHasSensorTimes(s, 2); }));

  state.resetFlag.store(true, std::memory_order_relaxed);
  ASSERT_TRUE(waitForSnapshot([](const std::string &s) { return allSensorTimesAre(s, 1); }));
  EXPECT_FALSE(state.resetFlag.load());
}

TEST_F(RunnerResetTest, ResetPreservesDeviceList) {
  createFakeHwmon();
  startRunner();

  ASSERT_TRUE(waitForSnapshot([](const std::string &s) { return snapshotHasSensorTimes(s, 2); }));

  state.resetFlag.store(true, std::memory_order_relaxed);
  ASSERT_TRUE(waitForSnapshot([](const std::string &s) { return allSensorTimesAre(s, 1); }));

  const auto snap = state.snapshot.load();
  ASSERT_TRUE(snap);
  const json devices = json::parse(*snap);
  ASSERT_TRUE(devices.is_array());
  ASSERT_EQ(devices.size(), 1u);
  EXPECT_EQ(devices[0]["name"], "testchip");
  EXPECT_TRUE(devices[0].contains("sensors"));
}
