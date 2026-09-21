#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "../Devices/SysfsDevice.hpp"
#include "../Sensor/SensorType.hpp"

namespace fs = std::filesystem;

namespace {

class SysfsDeviceTest : public ::testing::Test {
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

TEST_F(SysfsDeviceTest, EmptyDeviceSerializesWithNoSensors) {
  SysfsDevice device{"empty", DeviceType::CPU, dir};
  device.read();
  const nlohmann::json j = device.serialize();

  EXPECT_EQ(j["name"], "empty");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(DeviceType::CPU));
  EXPECT_FALSE(j.contains("sensors"));
}

TEST_F(SysfsDeviceTest, ReadsNameFromHwmonNameFile) {
  writeFile("name", "mychip");
  writeFile("temp1_input", "42000");

  SysfsDevice device{"fallback", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read();
  const nlohmann::json j = device.serialize();

  EXPECT_EQ(j["name"], "mychip");
  ASSERT_TRUE(j.contains("sensors"));
  EXPECT_FALSE(findSensor(j["sensors"], "temp1").empty());
}

TEST_F(SysfsDeviceTest, InitializeDiscoversSensorsFromDirectory) {
  writeFile("temp1_input", "30000");

  SysfsDevice device{"hwmon0", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read();
  const nlohmann::json j = device.serialize();

  ASSERT_TRUE(j.contains("sensors"));
  const nlohmann::json temp = findSensor(j["sensors"], "temp1");
  ASSERT_FALSE(temp.empty());
  EXPECT_EQ(temp["type"].get<int>(), static_cast<int>(SensorType::TEMPERATURE));
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 30.0f);
}

// -------------------------------------------------------------------------
// Attributes that exist but don't yield a number. Some drivers return EIO or
// an empty string for an attribute that is present, so a sensor reading has to
// be allowed to fail per cycle without taking anything else down.
// -------------------------------------------------------------------------

TEST_F(SysfsDeviceTest, UnreadableSensorYieldsNoSampleButStaysListed) {
  writeFile("temp1_input", "garbage");

  SysfsDevice device{"hwmon0", DeviceType::UNKNOWN, dir};
  device.initialize();
  ASSERT_NO_THROW(device.read());

  const nlohmann::json temp = findSensor(device.serialize()["sensors"], "temp1");
  ASSERT_FALSE(temp.empty());
  EXPECT_EQ(temp["readings"]["times"].get<std::size_t>(), 0u);
  EXPECT_TRUE(std::isnan(temp["readings"]["value"].get<double>()));
}

TEST_F(SysfsDeviceTest, EmptySensorFileYieldsNoSample) {
  writeFile("temp1_input", "");

  SysfsDevice device{"hwmon0", DeviceType::UNKNOWN, dir};
  device.initialize();
  ASSERT_NO_THROW(device.read());

  const nlohmann::json temp = findSensor(device.serialize()["sensors"], "temp1");
  ASSERT_FALSE(temp.empty());
  EXPECT_EQ(temp["readings"]["times"].get<std::size_t>(), 0u);
}

TEST_F(SysfsDeviceTest, SensorRecoversOnceFileBecomesReadable) {
  writeFile("temp1_input", "garbage");

  SysfsDevice device{"hwmon0", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read();

  writeFile("temp1_input", "40000");
  device.read();

  const nlohmann::json temp = findSensor(device.serialize()["sensors"], "temp1");
  EXPECT_EQ(temp["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 40.0f);
}

TEST_F(SysfsDeviceTest, BadSensorDoesNotAffectHealthySiblings) {
  writeFile("temp1_input", "garbage");
  writeFile("temp2_input", "35000");

  SysfsDevice device{"hwmon0", DeviceType::UNKNOWN, dir};
  device.initialize();
  ASSERT_NO_THROW(device.read());

  const nlohmann::json sensors = device.serialize()["sensors"];
  EXPECT_EQ(findSensor(sensors, "temp1")["readings"]["times"].get<std::size_t>(), 0u);

  const nlohmann::json healthy = findSensor(sensors, "temp2");
  ASSERT_FALSE(healthy.empty());
  EXPECT_EQ(healthy["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FLOAT_EQ(healthy["readings"]["value"].get<float>(), 35.0f);
}

TEST_F(SysfsDeviceTest, BadReadKeepsEarlierAggregates) {
  writeFile("temp1_input", "30000");

  SysfsDevice device{"hwmon0", DeviceType::UNKNOWN, dir};
  device.initialize();
  device.read();

  writeFile("temp1_input", "not_a_number");
  ASSERT_NO_THROW(device.read());

  const nlohmann::json temp = findSensor(device.serialize()["sensors"], "temp1");
  EXPECT_EQ(temp["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 30.0f);
}

// A sensor file that can't be opened at all is a different case: SourceFile
// throws from its constructor, and nothing between here and Runner::setup
// catches it, so the whole device fails to initialize.
TEST_F(SysfsDeviceTest, UnopenableSensorFileAbortsInitialize) {
  if (::geteuid() == 0) {
    GTEST_SKIP() << "root bypasses the permission check";
  }
  writeFile("temp1_input", "30000");
  ASSERT_EQ(::chmod((dir / "temp1_input").c_str(), 0), 0);

  SysfsDevice device{"hwmon0", DeviceType::UNKNOWN, dir};
  EXPECT_THROW(device.initialize(), std::runtime_error);
}
