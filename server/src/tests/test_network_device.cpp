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
#include "../SensorType.hpp"

namespace fs = std::filesystem;

namespace {

class NetworkDeviceTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    path = fs::temp_directory_path() / ("hwmon_netdev_test_" + std::to_string(::getpid()) + "_" +
                                        std::to_string(counter.fetch_add(1)));
    { std::ofstream touch{path}; }
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove(path, ec);
  }

  void writeNetDev(unsigned long long loRx, unsigned long long loTx, unsigned long long ethRx,
                   unsigned long long ethTx) {
    std::ofstream f{path, std::ios::trunc};
    f << "Inter-|   Receive                                                |  Transmit\n"
      << " face |bytes    packets errs drop fifo frame compressed multicast|"
      << "bytes    packets errs drop fifo colls carrier compressed\n"
      << "    lo: " << loRx << " 1 0 0 0 0 0 0 " << loTx << " 1 0 0 0 0 0 0\n"
      << "  eth0: " << ethRx << " 2 0 0 0 0 0 0 " << ethTx << " 3 0 0 0 0 0 0\n";
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

  fs::path path;
};

} // namespace

TEST_F(NetworkDeviceTest, ParseDataReadsRxTxAndTrimsIfaceNames) {
  writeNetDev(100, 200, 1000, 2000);
  NetworkDevice device{"net", path};

  const auto ifaces = device.parseData();

  ASSERT_EQ(ifaces.size(), 2u);
  ASSERT_TRUE(ifaces.contains("lo"));
  ASSERT_TRUE(ifaces.contains("eth0"));
  EXPECT_EQ(ifaces.at("lo")[0], 100u);
  EXPECT_EQ(ifaces.at("lo")[1], 200u);
  EXPECT_EQ(ifaces.at("eth0")[0], 1000u);
  EXPECT_EQ(ifaces.at("eth0")[1], 2000u);
}

TEST_F(NetworkDeviceTest, ParseDataCanBeCalledTwiceOnTheSameFile) {
  writeNetDev(100, 200, 1000, 2000);
  NetworkDevice device{"net", path};

  const auto first = device.parseData();
  writeNetDev(150, 250, 1100, 2100);
  const auto second = device.parseData();

  EXPECT_EQ(first.at("eth0")[0], 1000u);
  EXPECT_EQ(second.at("eth0")[0], 1100u);
  EXPECT_EQ(second.at("lo")[1], 250u);
}

TEST_F(NetworkDeviceTest, InitializeCreatesDownloadAndUploadSensors) {
  writeNetDev(100, 200, 1000, 2000);
  NetworkDevice device{"net", path};
  device.initialize();

  const nlohmann::json j = device.serialize();
  EXPECT_EQ(j["name"], "net");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(DeviceType::NETWORK));
  ASSERT_TRUE(j.contains("sensors"));
  ASSERT_EQ(j["sensors"].size(), 4u);

  for (const char *name : {"lo download", "lo upload", "eth0 download", "eth0 upload"}) {
    const nlohmann::json sensor = findSensor(j["sensors"], name);
    ASSERT_FALSE(sensor.empty()) << name;
    EXPECT_EQ(sensor["type"].get<int>(), static_cast<int>(SensorType::THROUGHPUT));
  }
}

TEST_F(NetworkDeviceTest, FirstReadAfterInitializeIsBaselineOnly) {
  writeNetDev(100, 200, 1000, 2000);
  NetworkDevice device{"net", path};
  device.initialize();
  device.read();

  const nlohmann::json j = device.serialize();
  for (const auto &sensor : j["sensors"]) {
    EXPECT_EQ(sensor["readings"]["times"].get<std::size_t>(), 0u) << sensor["name"];
  }
}

// initialize() consumes the file; a later read() must rewind and still push values
TEST_F(NetworkDeviceTest, ReadAfterInitializeProducesThroughputOnSecondSample) {
  writeNetDev(1'000, 2'000, 10'000, 20'000);
  NetworkDevice device{"net", path};
  device.initialize();
  device.read(); // baseline

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeNetDev(2'000, 3'000, 12'000, 24'000);
  device.read();

  const nlohmann::json j = device.serialize();
  const nlohmann::json ethDown = findSensor(j["sensors"], "eth0 download");
  const nlohmann::json ethUp = findSensor(j["sensors"], "eth0 upload");
  ASSERT_FALSE(ethDown.empty());
  ASSERT_FALSE(ethUp.empty());
  EXPECT_EQ(ethDown["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_EQ(ethUp["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_GT(ethDown["readings"]["value"].get<double>(), 0.0);
  EXPECT_GT(ethUp["readings"]["value"].get<double>(), 0.0);
}

TEST_F(NetworkDeviceTest, ResetClearsAggregatesAndReestablishesBaseline) {
  writeNetDev(1'000, 2'000, 10'000, 20'000);
  NetworkDevice device{"net", path};
  device.initialize();
  device.read();
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeNetDev(2'000, 3'000, 12'000, 24'000);
  device.read();
  ASSERT_EQ(findSensor(device.serialize()["sensors"], "eth0 download")["readings"]["times"]
                .get<std::size_t>(),
            1u);

  device.resetReadings();
  const nlohmann::json cleared = findSensor(device.serialize()["sensors"], "eth0 download");
  EXPECT_EQ(cleared["readings"]["times"].get<std::size_t>(), 0u);

  writeNetDev(2'000, 3'000, 12'000, 24'000);
  device.read(); // baseline again
  EXPECT_EQ(findSensor(device.serialize()["sensors"], "eth0 download")["readings"]["times"]
                .get<std::size_t>(),
            0u);

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeNetDev(3'000, 4'000, 14'000, 28'000);
  device.read();
  EXPECT_EQ(findSensor(device.serialize()["sensors"], "eth0 download")["readings"]["times"]
                .get<std::size_t>(),
            1u);
  EXPECT_GT(findSensor(device.serialize()["sensors"], "eth0 download")["readings"]["value"]
                .get<double>(),
            0.0);
}
