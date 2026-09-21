#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../Sensor/Sensor.hpp"
#include "../Sensor/SensorReading.hpp"
#include "../Sensor/SensorType.hpp"
#include "../Sensor/SourceFile.hpp"
#include "../Sensor/TransformDelta.hpp"
#include "../Sensor/TransformScale.hpp"
#include "helpers.hpp"

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

  // returns sensor instead of adding it to a global vector
  template <std::derived_from<Transform> TTransform>
  Sensor makeSensor(std::string name, SensorType type, unsigned int rawDivider = 0) {
    SensorConfig config{std::move(name), type, rawDivider};
    helpers::SensorVec sensors;
    Sensor::makeFileSensor<TTransform>(sensors, path, config);

    return std::move(*sensors[0]);
  }

  fs::path path;
};

} // namespace

TEST_F(SensorFileTest, ScalesRawValueByDivider) {
  writeRaw("30500\n");
  Sensor sensor = makeSensor<TransformScale>("cpu", SensorType::TEMPERATURE);
  sensor.updateValue();

  const SensorReading r = sensor.getReadings();
  EXPECT_DOUBLE_EQ(r.value, 30.5);
  EXPECT_DOUBLE_EQ(r.min_value, 30.5);
  EXPECT_DOUBLE_EQ(r.max_value, 30.5);
  EXPECT_EQ(r.times, 1);
}

TEST_F(SensorFileTest, FanSpeedIsNotScaled) {
  writeRaw("1200");
  Sensor sensor = makeSensor<TransformScale>("fan", SensorType::FAN_SPEED);
  sensor.updateValue();
  EXPECT_DOUBLE_EQ(sensor.getReadings().value, 1200.0);
}

TEST_F(SensorFileTest, ExplicitDividerOverridesTypeDefault) {
  writeRaw("2500000");
  Sensor sensor = makeSensor<TransformScale>("core", SensorType::FREQUENCY, 1000);
  sensor.updateValue();
  EXPECT_DOUBLE_EQ(sensor.getReadings().value, 2500.0);
}

TEST_F(SensorFileTest, AggregatesMinMaxSumAcrossReads) {
  writeRaw("30000");
  Sensor sensor = makeSensor<TransformScale>("cpu", SensorType::TEMPERATURE);

  sensor.updateValue(); // 30
  writeRaw("31000");
  sensor.updateValue(); // 31
  writeRaw("29000");
  sensor.updateValue(); // 29

  const SensorReading r = sensor.getReadings();
  EXPECT_DOUBLE_EQ(r.value, 29.0);
  EXPECT_DOUBLE_EQ(r.min_value, 29.0);
  EXPECT_DOUBLE_EQ(r.max_value, 31.0);
  EXPECT_DOUBLE_EQ(r.sum, 90.0);
  EXPECT_EQ(r.times, 3u);
}

TEST_F(SensorFileTest, SerializeEmitsNameTypeAndReadings) {
  writeRaw("45000");
  Sensor sensor = makeSensor<TransformScale>("core", SensorType::TEMPERATURE);
  sensor.updateValue();

  const nlohmann::json j = sensor.serialize();
  EXPECT_EQ(j["name"], "core");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(SensorType::TEMPERATURE));
  EXPECT_FALSE(j["isPrimary"].get<bool>());
  EXPECT_EQ(j["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_DOUBLE_EQ(j["readings"]["value"].get<double>(), 45.0);
}

TEST_F(SensorFileTest, SetPrimaryIsReflectedInSerialization) {
  writeRaw("45000");
  Sensor sensor = makeSensor<TransformScale>("core", SensorType::TEMPERATURE);
  sensor.setPrimary(true);
  EXPECT_TRUE(sensor.serialize()["isPrimary"].get<bool>());
}

TEST_F(SensorFileTest, ResetReadingsClearsAggregates) {
  writeRaw("30000");
  Sensor sensor = makeSensor<TransformScale>("cpu", SensorType::TEMPERATURE);
  sensor.updateValue();
  writeRaw("31000");
  sensor.updateValue();
  ASSERT_EQ(sensor.getReadings().times, 2u);

  sensor.resetReadings();
  const SensorReading cleared = sensor.getReadings();
  EXPECT_EQ(cleared.times, 0u);
  EXPECT_TRUE(std::isnan(cleared.value));
  EXPECT_TRUE(std::isnan(cleared.min_value));
  EXPECT_TRUE(std::isnan(cleared.max_value));
  EXPECT_DOUBLE_EQ(cleared.sum, 0.0);

  writeRaw("32000");
  sensor.updateValue();
  EXPECT_EQ(sensor.getReadings().times, 1u);
  EXPECT_DOUBLE_EQ(sensor.getReadings().value, 32.0);
}

TEST_F(SensorFileTest, DeltaFirstReadProducesNoSample) {
  writeRaw("1000000");
  Sensor sensor = makeSensor<TransformDelta>("rapl", SensorType::ENERGY);
  sensor.updateValue();

  // The first reading only establishes a baseline; nothing is recorded yet.
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

TEST_F(SensorFileTest, DeltaComputesPositivePowerFromDelta) {
  writeRaw("1000000");
  Sensor sensor = makeSensor<TransformDelta>("rapl", SensorType::ENERGY);
  sensor.updateValue(); // baseline

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeRaw("3000000"); // consumed 2,000,000 uJ since baseline
  sensor.updateValue();

  const SensorReading r = sensor.getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_FALSE(std::isnan(r.value));
  EXPECT_TRUE(std::isfinite(r.value));
  EXPECT_GT(r.value, 0.0);
}

TEST_F(SensorFileTest, DeltaReportsNegativePowerOnCounterReset) {
  writeRaw("1000000");
  Sensor sensor = makeSensor<TransformDelta>("rapl", SensorType::ENERGY);
  sensor.updateValue(); // baseline

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeRaw("500000"); // counter dropped, e.g. RAPL reset after suspend
  sensor.updateValue();

  const SensorReading r = sensor.getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_TRUE(std::isfinite(r.value));
  // Current behaviour: negative delta is reported as negative power.
  EXPECT_LT(r.value, 0.0);
}

TEST_F(SensorFileTest, SurvivesEmptySensorRead) {
  writeRaw(""); // empty read, e.g. transient sysfs state
  Sensor sensor = makeSensor<TransformScale>("cpu", SensorType::TEMPERATURE);
  EXPECT_NO_THROW(sensor.updateValue());
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

TEST_F(SensorFileTest, SurvivesNonNumericSensorRead) {
  writeRaw("garbage");
  Sensor sensor = makeSensor<TransformScale>("cpu", SensorType::TEMPERATURE);
  EXPECT_NO_THROW(sensor.updateValue());
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

TEST_F(SensorFileTest, RejectsTrailingGarbageAfterNumber) {
  writeRaw("30000abc");
  Sensor sensor = makeSensor<TransformScale>("cpu", SensorType::TEMPERATURE);
  sensor.updateValue();
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

TEST_F(SensorFileTest, BadReadDoesNotClobberPreviousAggregates) {
  writeRaw("30000");
  Sensor sensor = makeSensor<TransformScale>("cpu", SensorType::TEMPERATURE);
  sensor.updateValue(); // good sample: 30.0

  writeRaw("not_a_number");
  EXPECT_NO_THROW(sensor.updateValue()); // bad read must be ignored

  const SensorReading r = sensor.getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_DOUBLE_EQ(r.value, 30.0);
}

TEST_F(SensorFileTest, DeltaSurvivesNonNumericRead) {
  writeRaw("garbage");
  Sensor sensor = makeSensor<TransformDelta>("rapl", SensorType::ENERGY);
  EXPECT_NO_THROW(sensor.updateValue());
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

TEST_F(SensorFileTest, DeltaResetClearsAggregatesAndBaseline) {
  writeRaw("1000000");
  Sensor sensor = makeSensor<TransformDelta>("rapl", SensorType::ENERGY);
  sensor.updateValue(); // baseline

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeRaw("3000000");
  sensor.updateValue();
  ASSERT_EQ(sensor.getReadings().times, 1u);
  const double valueBeforeReset = sensor.getReadings().value;

  sensor.resetReadings();
  const SensorReading cleared = sensor.getReadings();
  EXPECT_EQ(cleared.times, 0u);
  EXPECT_TRUE(std::isnan(cleared.value));
  EXPECT_TRUE(std::isnan(cleared.min_value));
  EXPECT_TRUE(std::isnan(cleared.max_value));
  EXPECT_DOUBLE_EQ(cleared.sum, 0.0);

  sensor.updateValue(); // re-establish baseline from current counter
  EXPECT_EQ(sensor.getReadings().times, 0u);

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  writeRaw("5000000");
  sensor.updateValue();
  EXPECT_EQ(sensor.getReadings().times, 1u);
  EXPECT_GT(sensor.getReadings().value, 0.0);
  EXPECT_NE(sensor.getReadings().value, valueBeforeReset);
}

TEST_F(SensorFileTest, SourceFileThrowsOnMissingPath) {
  EXPECT_THROW(SourceFile{path / "does_not_exist"}, std::runtime_error);
}

TEST_F(SensorFileTest, SourceFileThrowsOnDirectory) {
  EXPECT_THROW(SourceFile{fs::temp_directory_path()}, std::runtime_error);
}

TEST_F(SensorFileTest, MakeFileSensorAppendsToVector) {
  writeRaw("42000");
  std::vector<std::unique_ptr<Sensor>> sensors;
  Sensor::makeFileSensor<TransformScale>(sensors, path, {"edge", SensorType::TEMPERATURE});

  ASSERT_EQ(sensors.size(), 1u);
  sensors.front()->updateValue();
  EXPECT_EQ(sensors.front()->getName(), "edge");
  EXPECT_DOUBLE_EQ(sensors.front()->getReadings().value, 42.0);
}
