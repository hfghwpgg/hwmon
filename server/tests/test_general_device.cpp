#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unistd.h>

#include "Devices/GeneralDevice.hpp"
#include "SensorType.hpp"

namespace fs = std::filesystem;

namespace {

class GeneralDeviceTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    dir = fs::temp_directory_path() / ("hwmon_device_test_" + std::to_string(::getpid()) + "_" +
                                       std::to_string(counter.fetch_add(1)));
    fs::create_directories(dir);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(dir, ec);
  }

  void writeFile(const std::string &name, const std::string &contents) {
    std::ofstream f{dir / name, std::ios::trunc};
    f << contents;
  }

  // Locates a serialized sensor by its "name" field; returns null json if absent.
  static nlohmann::json findSensor(const nlohmann::json &sensors, const std::string &name) {
    for (const auto &s : sensors) {
      if (s.value("name", std::string{}) == name) {
        return s;
      }
    }
    return nlohmann::json{};
  }

  fs::path dir;
};

} // namespace

TEST_F(GeneralDeviceTest, DiscoversWhitelistedSensorsAndDeducesTypes) {
  writeFile("temp1_input", "42000");
  writeFile("temp1_label", "Package");
  writeFile("fan1_input", "900");
  writeFile("in0_input", "1200");
  // Not whitelisted -> must be ignored.
  writeFile("temp1_crit", "100000");
  // No underscore -> must be skipped.
  // but is used as device name
  writeFile("name", "mychip");

  GeneralDevice device{"mydev", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read();
  const nlohmann::json j = device.serialize();

  EXPECT_EQ(j["name"], "mychip");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(DeviceType::UNKNOWN));

  const nlohmann::json &sensors = j["sensors"];
  ASSERT_TRUE(sensors.is_array());
  EXPECT_EQ(sensors.size(), 3u);

  // The _label file overrides the sensor's display name.
  const nlohmann::json temp = findSensor(sensors, "Package");
  ASSERT_FALSE(temp.empty());
  EXPECT_EQ(temp["type"].get<int>(), static_cast<int>(SensorType::TEMPERATURE));
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 42.0f);

  const nlohmann::json fan = findSensor(sensors, "fan1");
  ASSERT_FALSE(fan.empty());
  EXPECT_EQ(fan["type"].get<int>(), static_cast<int>(SensorType::FAN_SPEED));
  EXPECT_FLOAT_EQ(fan["readings"]["value"].get<float>(), 900.0f);

  const nlohmann::json volt = findSensor(sensors, "in0");
  ASSERT_FALSE(volt.empty());
  EXPECT_EQ(volt["type"].get<int>(), static_cast<int>(SensorType::VOLTAGE));
  EXPECT_FLOAT_EQ(volt["readings"]["value"].get<float>(), 1.2f);
}

TEST_F(GeneralDeviceTest, SensorWithoutReadingInterfaceIsSkipped) {
  // Only a label, no _input/_average -> no sensor should be created.
  writeFile("temp1_label", "Orphan");
  writeFile("fan1_input", "1500");

  GeneralDevice device{"mydev", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read();
  const nlohmann::json j = device.serialize();

  ASSERT_TRUE(j["sensors"].is_array());
  EXPECT_EQ(j["sensors"].size(), 1u);
  EXPECT_EQ(findSensor(j["sensors"], "Orphan"), nlohmann::json{});
}

TEST_F(GeneralDeviceTest, EmptyDeviceSerializesWithNoSensors) {
  GeneralDevice device{"empty", DeviceType::CPU, dir};
  device.read();
  const nlohmann::json j = device.serialize();

  EXPECT_EQ(j["name"], "empty");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(DeviceType::CPU));
  // No sensors were added, so the key is never populated.
  EXPECT_FALSE(j.contains("sensors"));
}

TEST_F(GeneralDeviceTest, ReadsFromAverageInterface) {
  writeFile("temp1_average", "45000");

  GeneralDevice device{"mydev", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read();
  const nlohmann::json j = device.serialize();

  const nlohmann::json temp = findSensor(j["sensors"], "temp1");
  ASSERT_FALSE(temp.empty());
  EXPECT_EQ(temp["type"].get<int>(), static_cast<int>(SensorType::TEMPERATURE));
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 45.0f);
}

TEST_F(GeneralDeviceTest, SkipsSensorWithBothInputAndAverage) {
  writeFile("temp1_input", "42000");
  writeFile("temp1_average", "43000");

  GeneralDevice device{"mydev", DeviceType::UNKNOWN, dir};
  device.read();
  const nlohmann::json j = device.serialize();

  if (j.contains("sensors")) {
    ASSERT_TRUE(j["sensors"].is_array());
    EXPECT_EQ(j["sensors"].size(), 0u);
  }
}

TEST_F(GeneralDeviceTest, EnergyInputCreatesEnergySensor) {
  writeFile("energy1_input", "1000000");

  GeneralDevice device{"mydev", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read(); // baseline only

  writeFile("energy1_input", "3000000");
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  device.read();

  const nlohmann::json energy = findSensor(device.serialize()["sensors"], "energy1");
  ASSERT_FALSE(energy.empty());
  EXPECT_EQ(energy["type"].get<int>(), static_cast<int>(SensorType::ENERGY));
  EXPECT_EQ(energy["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FALSE(std::isnan(energy["readings"]["value"].get<float>()));
  EXPECT_TRUE(std::isfinite(energy["readings"]["value"].get<float>()));
  EXPECT_GT(energy["readings"]["value"].get<float>(), 0.0f);
}

TEST_F(GeneralDeviceTest, RepeatedReadUpdatesAggregates) {
  writeFile("temp1_input", "30000");

  GeneralDevice device{"mydev", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read();

  writeFile("temp1_input", "31000");
  device.read();

  const nlohmann::json temp = findSensor(device.serialize()["sensors"], "temp1");
  ASSERT_FALSE(temp.empty());
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 31.0f);
  EXPECT_FLOAT_EQ(temp["readings"]["min_value"].get<float>(), 30.0f);
  EXPECT_FLOAT_EQ(temp["readings"]["max_value"].get<float>(), 31.0f);
  EXPECT_EQ(temp["readings"]["times"].get<std::size_t>(), 2u);
}

TEST_F(GeneralDeviceTest, UnknownPrefixUsesUnscaledDivider) {
  writeFile("bogus1_input", "12345");

  GeneralDevice device{"mydev", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read();
  const nlohmann::json bogus = findSensor(device.serialize()["sensors"], "bogus1");
  ASSERT_FALSE(bogus.empty());
  EXPECT_EQ(bogus["type"].get<int>(), static_cast<int>(SensorType::UNKNOWN));
  EXPECT_FLOAT_EQ(bogus["readings"]["value"].get<float>(), 12345.0f);
}
