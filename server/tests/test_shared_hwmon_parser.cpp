#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "Devices/SharedHwmonParser.hpp"
#include "Sensor.hpp"
#include "SensorType.hpp"

namespace fs = std::filesystem;

namespace {

class SharedHwmonParserTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    dir = fs::temp_directory_path() / ("hwmon_parser_test_" + std::to_string(::getpid()) + "_" +
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

  nlohmann::json createReadAndSerialize() {
    const auto map = SharedHwmonParser::parseHwmonDirectory(dir);
    std::vector<std::unique_ptr<Sensor>> sensors;
    SharedHwmonParser::createSensors(dir, map, sensors);
    for (auto &s : sensors) {
      s->updateValue();
    }
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &s : sensors) {
      arr.push_back(s->serialize());
    }
    return arr;
  }

  std::vector<std::unique_ptr<Sensor>> createSensors() {
    const auto map = SharedHwmonParser::parseHwmonDirectory(dir);
    std::vector<std::unique_ptr<Sensor>> sensors;
    SharedHwmonParser::createSensors(dir, map, sensors);
    return sensors;
  }

  fs::path dir;
};

} // namespace

TEST_F(SharedHwmonParserTest, ParseIgnoresNonWhitelistedAndUnderscorelessFiles) {
  writeFile("temp1_input", "42000");
  writeFile("temp1_crit", "100000");
  writeFile("name", "mychip");

  const auto map = SharedHwmonParser::parseHwmonDirectory(dir);
  ASSERT_EQ(map.size(), 1u);
  ASSERT_TRUE(map.contains("temp1"));
  EXPECT_EQ(map.at("temp1"), std::vector<std::string>{"input"});
}

TEST_F(SharedHwmonParserTest, ParseAndCreateWhitelistedSensors) {
  writeFile("temp1_input", "42000");
  writeFile("temp1_label", "Package");
  writeFile("fan1_input", "900");
  writeFile("in0_input", "1200");

  const nlohmann::json sensors = createReadAndSerialize();
  ASSERT_TRUE(sensors.is_array());
  EXPECT_EQ(sensors.size(), 3u);

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

TEST_F(SharedHwmonParserTest, SkipsSensorWithoutInputOrAverage) {
  writeFile("temp1_label", "Orphan");
  writeFile("fan1_input", "1500");

  const nlohmann::json sensors = createReadAndSerialize();
  ASSERT_TRUE(sensors.is_array());
  EXPECT_EQ(sensors.size(), 1u);
  EXPECT_EQ(findSensor(sensors, "Orphan"), nlohmann::json{});
}

TEST_F(SharedHwmonParserTest, CreatesSensorFromAverage) {
  writeFile("temp1_average", "45000");

  const nlohmann::json sensors = createReadAndSerialize();
  const nlohmann::json temp = findSensor(sensors, "temp1");
  ASSERT_FALSE(temp.empty());
  EXPECT_EQ(temp["type"].get<int>(), static_cast<int>(SensorType::TEMPERATURE));
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 45.0f);
}

TEST_F(SharedHwmonParserTest, SkipsSensorWithBothInputAndAverage) {
  writeFile("temp1_input", "42000");
  writeFile("temp1_average", "43000");

  const nlohmann::json sensors = createReadAndSerialize();
  ASSERT_TRUE(sensors.is_array());
  EXPECT_EQ(sensors.size(), 0u);
}

TEST_F(SharedHwmonParserTest, CreatesEnergySensor) {
  writeFile("energy1_input", "1000000");

  auto sensors = createSensors();
  ASSERT_EQ(sensors.size(), 1u);
  sensors[0]->updateValue(); // baseline

  writeFile("energy1_input", "3000000");
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensors[0]->updateValue();

  const nlohmann::json energy = sensors[0]->serialize();
  EXPECT_EQ(energy["type"].get<int>(), static_cast<int>(SensorType::ENERGY));
  EXPECT_EQ(energy["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FALSE(std::isnan(energy["readings"]["value"].get<float>()));
  EXPECT_TRUE(std::isfinite(energy["readings"]["value"].get<float>()));
  EXPECT_GT(energy["readings"]["value"].get<float>(), 0.0f);
}

TEST_F(SharedHwmonParserTest, RepeatedReadUpdatesAggregates) {
  writeFile("temp1_input", "30000");

  auto sensors = createSensors();
  ASSERT_EQ(sensors.size(), 1u);
  sensors[0]->updateValue();

  writeFile("temp1_input", "31000");
  sensors[0]->updateValue();

  const nlohmann::json temp = sensors[0]->serialize();
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 31.0f);
  EXPECT_FLOAT_EQ(temp["readings"]["min_value"].get<float>(), 30.0f);
  EXPECT_FLOAT_EQ(temp["readings"]["max_value"].get<float>(), 31.0f);
  EXPECT_EQ(temp["readings"]["times"].get<std::size_t>(), 2u);
}

TEST_F(SharedHwmonParserTest, UnknownPrefixUsesUnscaledDivider) {
  writeFile("bogus1_input", "12345");

  const nlohmann::json sensors = createReadAndSerialize();
  const nlohmann::json bogus = findSensor(sensors, "bogus1");
  ASSERT_FALSE(bogus.empty());
  EXPECT_EQ(bogus["type"].get<int>(), static_cast<int>(SensorType::UNKNOWN));
  EXPECT_FLOAT_EQ(bogus["readings"]["value"].get<float>(), 12345.0f);
}
