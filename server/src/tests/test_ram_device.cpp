#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <unistd.h>

#include "../DeviceType.hpp"
#include "../Devices/RamDevice.hpp"
#include "../Sensor/SensorType.hpp"

namespace fs = std::filesystem;

namespace {

class RamDeviceTest : public ::testing::Test {
protected:
  void SetUp() override {
    path = fs::temp_directory_path() / ("hwmon_ram_test_" + std::to_string(::getpid()));
    write(1000, 400);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove(path, ec);
  }
  void write(long total, long avail) {
    std::ofstream f{path, std::ios::trunc};
    f << "MemTotal:       " << total << " kB\nMemFree:         1 kB\nMemAvailable:   " << avail
      << " kB\nBuffers:         1 kB\n";
  }
  static nlohmann::json find(const nlohmann::json &sensors, const std::string &name) {
    for (const auto &s : sensors)
      if (s["name"] == name)
        return s;
    return nlohmann::json{};
  }
  fs::path path;
};

} // namespace

TEST_F(RamDeviceTest, ReadsTotalAndAvailableInBytes) {
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  const auto j = dev.serialize();
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(DeviceType::RAM));
  ASSERT_EQ(j["sensors"].size(), 2u);
  const auto total = find(j["sensors"], "Total memory");
  const auto used = find(j["sensors"], "Used memory");
  EXPECT_EQ(total["type"].get<int>(), static_cast<int>(SensorType::MEMORY));
  EXPECT_DOUBLE_EQ(total["readings"]["value"].get<double>(), 1000 * 1024.0);
  EXPECT_DOUBLE_EQ(used["readings"]["value"].get<double>(), (1000 - 400) * 1024.0);
}

TEST_F(RamDeviceTest, TotalDoesNotAggregateButUsedDoes) {
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  write(1000, 300);
  dev.read();
  dev.read();
  const auto j = dev.serialize();
  const auto total = find(j["sensors"], "Total memory");
  const auto used = find(j["sensors"], "Used memory");
  EXPECT_EQ(total["readings"]["times"].get<size_t>(), 1u);
  EXPECT_EQ(used["readings"]["times"].get<size_t>(), 3u);
  EXPECT_DOUBLE_EQ(used["readings"]["value"].get<double>(), 700 * 1024.0);
  EXPECT_DOUBLE_EQ(used["readings"]["min_value"].get<double>(), 600 * 1024.0);
  EXPECT_DOUBLE_EQ(used["readings"]["max_value"].get<double>(), 700 * 1024.0);
}

TEST_F(RamDeviceTest, UsedSensorIsPrimaryTotalIsNot) {
  RamDevice dev{path};
  dev.initialize();
  const auto j = dev.serialize();
  EXPECT_FALSE(find(j["sensors"], "Total memory")["isPrimary"].get<bool>());
  EXPECT_TRUE(find(j["sensors"], "Used memory")["isPrimary"].get<bool>());
}

TEST_F(RamDeviceTest, EmptyFileProducesNoReadings) {
  { std::ofstream f{path, std::ios::trunc}; }
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  for (const auto &s : dev.serialize()["sensors"])
    EXPECT_EQ(s["readings"]["times"].get<size_t>(), 0u) << s["name"];
}

TEST_F(RamDeviceTest, MissingMemAvailableProducesNoReadings) {
  { std::ofstream f{path, std::ios::trunc}; f << "MemTotal: 1000 kB\nMemFree: 5 kB\n"; }
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  for (const auto &s : dev.serialize()["sensors"])
    EXPECT_EQ(s["readings"]["times"].get<size_t>(), 0u) << s["name"];
}

TEST_F(RamDeviceTest, MissingMemTotalProducesNoReadings) {
  { std::ofstream f{path, std::ios::trunc}; f << "MemAvailable: 400 kB\n"; }
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  for (const auto &s : dev.serialize()["sensors"])
    EXPECT_EQ(s["readings"]["times"].get<size_t>(), 0u) << s["name"];
}

TEST_F(RamDeviceTest, GarbageValuesProduceNoReadings) {
  { std::ofstream f{path, std::ios::trunc}; f << "MemTotal: abc kB\nMemAvailable: xyz kB\n"; }
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  for (const auto &s : dev.serialize()["sensors"])
    EXPECT_EQ(s["readings"]["times"].get<size_t>(), 0u) << s["name"];
}

TEST_F(RamDeviceTest, LinesWithoutColonAreIgnored) {
  { std::ofstream f{path, std::ios::trunc}; f << "junk line\nMemTotal: 1000 kB\nmore junk\nMemAvailable: 400 kB\n"; }
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  EXPECT_DOUBLE_EQ(
      find(dev.serialize()["sensors"], "Used memory")["readings"]["value"].get<double>(),
      600 * 1024.0);
}

TEST_F(RamDeviceTest, AvailableGreaterThanTotalIsRejected) {
  write(400, 1000);
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  for (const auto &s : dev.serialize()["sensors"])
    EXPECT_EQ(s["readings"]["times"].get<size_t>(), 0u) << s["name"];
}

TEST_F(RamDeviceTest, BadReadAfterGoodReadKeepsLastValue) {
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  { std::ofstream f{path, std::ios::trunc}; }
  dev.read();
  const auto used = find(dev.serialize()["sensors"], "Used memory");
  EXPECT_EQ(used["readings"]["times"].get<size_t>(), 1u);
  EXPECT_DOUBLE_EQ(used["readings"]["value"].get<double>(), 600 * 1024.0);
}

TEST_F(RamDeviceTest, FileRemovedAfterInitializeDoesNotThrow) {
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  fs::remove(path);
  EXPECT_NO_THROW(dev.read());
  EXPECT_EQ(find(dev.serialize()["sensors"], "Used memory")["readings"]["times"].get<size_t>(),
            1u);
}

TEST_F(RamDeviceTest, ResetClearsReadings) {
  RamDevice dev{path};
  dev.initialize();
  dev.read();
  dev.resetReadings();
  for (const auto &s : dev.serialize()["sensors"])
    EXPECT_EQ(s["readings"]["times"].get<size_t>(), 0u) << s["name"];
  dev.read();
  EXPECT_EQ(find(dev.serialize()["sensors"], "Used memory")["readings"]["times"].get<size_t>(),
            1u);
}

TEST_F(RamDeviceTest, MissingFileYieldsNoSensors) {
  RamDevice dev{path.string() + "_nope"};
  dev.initialize();
  dev.read();
  EXPECT_FALSE(dev.serialize().contains("sensors"));
}
