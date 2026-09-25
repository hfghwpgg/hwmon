#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unistd.h>

#include "../DeviceType.hpp"
#include "../Devices/NetworkDevice.hpp"
#include "../Sensor/SensorType.hpp"

namespace fs = std::filesystem;

namespace {

class NetworkDeviceTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    statsPath = fs::temp_directory_path() /
                ("hwmon_netdev_test_" + std::to_string(::getpid()) + "_" +
                 std::to_string(counter.fetch_add(1)));
    fs::create_directory(statsPath);
    writeCounters(0, 0);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(statsPath, ec);
  }

  void writeCounters(unsigned long long rx, unsigned long long tx) {
    writeFile(statsPath / "rx_bytes", rx);
    writeFile(statsPath / "tx_bytes", tx);
  }

  static void writeFile(const fs::path &path, unsigned long long value) {
    std::ofstream f{path, std::ios::trunc};
    f << value << '\n';
    f.flush();
  }

  static nlohmann::json findSensor(const nlohmann::json &sensors, const std::string &name) {
    for (const auto &s : sensors) {
      if (s.value("name", std::string{}) == name) {
        return s;
      }
    }
    return nlohmann::json{};
  }

  fs::path statsPath;
};

} // namespace

TEST_F(NetworkDeviceTest, InitializeCreatesReceiveAndTransmitSensors) {
  NetworkDevice device{"eth0", statsPath};
  device.initialize();

  const nlohmann::json j = device.serialize();
  EXPECT_EQ(j["name"], "eth0");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(DeviceType::NETWORK));
  ASSERT_TRUE(j.contains("sensors"));
  ASSERT_EQ(j["sensors"].size(), 2u);

  for (const char *name : {"Recieve speed", "Transmit speed"}) {
    const nlohmann::json sensor = findSensor(j["sensors"], name);
    ASSERT_FALSE(sensor.empty()) << name;
    EXPECT_EQ(sensor["type"].get<int>(), static_cast<int>(SensorType::THROUGHPUT));
  }
}

TEST_F(NetworkDeviceTest, FirstReadAfterInitializeIsBaselineOnly) {
  writeCounters(1000, 2000);
  NetworkDevice device{"eth0", statsPath};
  device.initialize();
  device.read();

  const nlohmann::json j = device.serialize();
  for (const auto &sensor : j["sensors"]) {
    EXPECT_EQ(sensor["readings"]["times"].get<std::size_t>(), 0u) << sensor["name"];
  }
}

TEST_F(NetworkDeviceTest, ReadProducesThroughputFromRxAndTxCounters) {
  writeCounters(1'000, 2'000);
  NetworkDevice device{"eth0", statsPath};
  device.initialize();
  device.read(); // baseline

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeCounters(3'000, 6'000);
  device.read();

  const nlohmann::json j = device.serialize();
  const nlohmann::json rx = findSensor(j["sensors"], "Recieve speed");
  const nlohmann::json tx = findSensor(j["sensors"], "Transmit speed");
  ASSERT_FALSE(rx.empty());
  ASSERT_FALSE(tx.empty());
  EXPECT_EQ(rx["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_EQ(tx["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_GT(rx["readings"]["value"].get<double>(), 0.0);
  EXPECT_GT(tx["readings"]["value"].get<double>(), 0.0);
}

TEST_F(NetworkDeviceTest, ResetClearsAggregatesAndReestablishesBaseline) {
  writeCounters(1'000, 2'000);
  NetworkDevice device{"eth0", statsPath};
  device.initialize();
  device.read();
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeCounters(3'000, 6'000);
  device.read();
  ASSERT_EQ(findSensor(device.serialize()["sensors"], "Recieve speed")["readings"]["times"]
                .get<std::size_t>(),
            1u);

  device.resetReadings();
  const nlohmann::json cleared = findSensor(device.serialize()["sensors"], "Recieve speed");
  EXPECT_EQ(cleared["readings"]["times"].get<std::size_t>(), 0u);

  writeCounters(3'000, 6'000);
  device.read(); // baseline again
  EXPECT_EQ(findSensor(device.serialize()["sensors"], "Recieve speed")["readings"]["times"]
                .get<std::size_t>(),
            0u);

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeCounters(5'000, 8'000);
  device.read();
  EXPECT_EQ(findSensor(device.serialize()["sensors"], "Recieve speed")["readings"]["times"]
                .get<std::size_t>(),
            1u);
  EXPECT_GT(findSensor(device.serialize()["sensors"], "Recieve speed")["readings"]["value"]
                .get<double>(),
            0.0);
}
