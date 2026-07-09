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

#include "EnergySensor.hpp"
#include "Sensor.hpp"
#include "SensorReading.hpp"
#include "SensorType.hpp"

namespace fs = std::filesystem;

namespace {

// Each test gets a unique temp file emulating a single hwmon attribute file.
class SensorFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    path = fs::temp_directory_path() / ("hwmon_sensor_test_" + std::to_string(::getpid()) + "_" +
                                        std::to_string(counter.fetch_add(1)));
    { std::ofstream touch{path}; }
    file = std::make_unique<std::ifstream>(path);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove(path, ec);
  }

  void writeRaw(const std::string &contents) {
    std::ofstream f{path, std::ios::trunc};
    f << contents;
    f.flush();
  }

  fs::path path;
  std::unique_ptr<std::ifstream> file;
};

} // namespace

TEST_F(SensorFileTest, ScalesRawValueByDivider) {
  writeRaw("30500\n");
  Sensor sensor{std::move(file), "cpu", SensorType::TEMPERATURE};
  sensor.updateValue();

  const SensorReading r = sensor.getReadings();
  EXPECT_DOUBLE_EQ(r.value, 30.5);
  EXPECT_DOUBLE_EQ(r.min_value, 30.5);
  EXPECT_DOUBLE_EQ(r.max_value, 30.5);
  EXPECT_EQ(r.times, 1);
}

TEST_F(SensorFileTest, FanSpeedIsNotScaled) {
  writeRaw("1200");
  Sensor sensor{std::move(file), "fan", SensorType::FAN_SPEED};
  sensor.updateValue();
  EXPECT_DOUBLE_EQ(sensor.getReadings().value, 1200.0f);
}

TEST_F(SensorFileTest, AggregatesMinMaxSumAcrossReads) {
  writeRaw("30000");
  Sensor sensor{std::move(file), "cpu", SensorType::TEMPERATURE};

  sensor.updateValue(); // 30
  writeRaw("31000");
  sensor.updateValue(); // 31
  writeRaw("29000");
  sensor.updateValue(); // 29

  const SensorReading r = sensor.getReadings();
  EXPECT_DOUBLE_EQ(r.value, 29.0f);
  EXPECT_DOUBLE_EQ(r.min_value, 29.0f);
  EXPECT_DOUBLE_EQ(r.max_value, 31.0f);
  EXPECT_DOUBLE_EQ(r.sum, 90.0);
  EXPECT_EQ(r.times, 3u);
}

TEST_F(SensorFileTest, SerializeEmitsNameTypeAndReadings) {
  writeRaw("45000");
  Sensor sensor{std::move(file), "core", SensorType::TEMPERATURE};
  sensor.updateValue();

  const nlohmann::json j = sensor.serialize();
  EXPECT_EQ(j["name"], "core");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(SensorType::TEMPERATURE));
  EXPECT_EQ(j["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_DOUBLE_EQ(j["readings"]["value"].get<float>(), 45.0f);
}

TEST_F(SensorFileTest, EnergySensorFirstReadProducesNoSample) {
  writeRaw("1000000");
  EnergySensor sensor{std::move(file), "rapl", SensorType::ENERGY};
  sensor.updateValue();

  // The first reading only establishes a baseline; nothing is recorded yet.
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

TEST_F(SensorFileTest, EnergySensorComputesPositivePowerFromDelta) {
  writeRaw("1000000");
  EnergySensor sensor{std::move(file), "rapl", SensorType::ENERGY};
  sensor.updateValue(); // baseline

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeRaw("3000000"); // consumed 2,000,000 uJ since baseline
  sensor.updateValue();

  const SensorReading r = sensor.getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_FALSE(std::isnan(r.value));
  EXPECT_TRUE(std::isfinite(r.value));
  EXPECT_GT(r.value, 0.0f);
}

TEST_F(SensorFileTest, EnergySensorReportsNegativePowerOnCounterReset) {
  writeRaw("1000000");
  EnergySensor sensor{std::move(file), "rapl", SensorType::ENERGY};
  sensor.updateValue(); // baseline

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeRaw("500000"); // counter dropped, e.g. RAPL reset after suspend
  sensor.updateValue();

  const SensorReading r = sensor.getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_TRUE(std::isfinite(r.value));
  // Current behaviour: negative delta is reported as negative power.
  EXPECT_LT(r.value, 0.0f);
}

TEST_F(SensorFileTest, SurvivesEmptySensorRead) {
  writeRaw(""); // empty read, e.g. transient sysfs state
  Sensor sensor{std::move(file), "cpu", SensorType::TEMPERATURE};
  EXPECT_NO_THROW(sensor.updateValue());
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

TEST_F(SensorFileTest, SurvivesNonNumericSensorRead) {
  writeRaw("garbage");
  Sensor sensor{std::move(file), "cpu", SensorType::TEMPERATURE};
  EXPECT_NO_THROW(sensor.updateValue());
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

TEST_F(SensorFileTest, BadReadDoesNotClobberPreviousAggregates) {
  writeRaw("30000");
  Sensor sensor{std::move(file), "cpu", SensorType::TEMPERATURE};
  sensor.updateValue(); // good sample: 30.0

  writeRaw("not_a_number");
  EXPECT_NO_THROW(sensor.updateValue()); // bad read must be ignored

  const SensorReading r = sensor.getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_DOUBLE_EQ(r.value, 30.0f);
}

TEST_F(SensorFileTest, EnergySensorSurvivesNonNumericRead) {
  writeRaw("garbage");
  EnergySensor sensor{std::move(file), "rapl", SensorType::ENERGY};
  EXPECT_NO_THROW(sensor.updateValue());
}
