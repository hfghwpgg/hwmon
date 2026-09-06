#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <unistd.h>

#include "Devices/SysfsDevice.hpp"
#include "SensorType.hpp"

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
