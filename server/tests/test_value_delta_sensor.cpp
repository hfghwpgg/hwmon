#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "../Sensor.hpp"
#include "../SensorType.hpp"
#include "../ValueDeltaSensor.hpp"

TEST(ValueDeltaSensorTest, FirstPushedValueOnlyEstablishesBaseline) {
  ValueDeltaSensor sensor{"eth0 download", SensorType::THROUGHPUT};

  sensor.setValue(1'000);
  sensor.updateValue();

  EXPECT_EQ(sensor.getReadings().times, 0u);
  EXPECT_TRUE(std::isnan(sensor.getReadings().value));
}

TEST(ValueDeltaSensorTest, SecondReadReportsPositiveRate) {
  ValueDeltaSensor sensor{"eth0 download", SensorType::THROUGHPUT};

  sensor.setValue(1'000);
  sensor.updateValue();

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensor.setValue(3'000);
  sensor.updateValue();

  const SensorReading r = sensor.getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_TRUE(std::isfinite(r.value));
  EXPECT_GT(r.value, 0.0);
}

TEST(ValueDeltaSensorTest, ReportsNegativeRateOnCounterReset) {
  ValueDeltaSensor sensor{"eth0 download", SensorType::THROUGHPUT};

  sensor.setValue(3'000);
  sensor.updateValue();

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensor.setValue(1'000);
  sensor.updateValue();

  const SensorReading r = sensor.getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_TRUE(std::isfinite(r.value));
  EXPECT_LT(r.value, 0.0);
}

TEST(ValueDeltaSensorTest, StaysNanUntilValueIsPushed) {
  ValueDeltaSensor sensor{"eth0 download", SensorType::THROUGHPUT};

  sensor.updateValue();

  EXPECT_TRUE(std::isnan(sensor.getReadings().value));
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

// a missing push must not wipe an already-established baseline
TEST(ValueDeltaSensorTest, MissingPushDoesNotClobberBaseline) {
  ValueDeltaSensor sensor{"eth0 download", SensorType::THROUGHPUT};

  sensor.setValue(1'000);
  sensor.updateValue(); // baseline
  sensor.updateValue(); // no setValue → NaN, ignored

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensor.setValue(2'000);
  sensor.updateValue();

  const SensorReading r = sensor.getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_GT(r.value, 0.0);
}

TEST(ValueDeltaSensorTest, ConsumesValueOnce) {
  ValueDeltaSensor sensor{"eth0 download", SensorType::THROUGHPUT};

  sensor.setValue(1'000);
  sensor.updateValue();
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensor.setValue(2'000);
  sensor.updateValue();
  ASSERT_EQ(sensor.getReadings().times, 1u);

  sensor.updateValue();
  EXPECT_EQ(sensor.getReadings().times, 1u);
}

TEST(ValueDeltaSensorTest, ResetClearsAggregatesAndBaseline) {
  ValueDeltaSensor sensor{"eth0 download", SensorType::THROUGHPUT};

  sensor.setValue(1'000);
  sensor.updateValue();
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensor.setValue(3'000);
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

  sensor.setValue(3'000);
  sensor.updateValue(); // re-establish baseline
  EXPECT_EQ(sensor.getReadings().times, 0u);

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensor.setValue(5'000);
  sensor.updateValue();
  EXPECT_EQ(sensor.getReadings().times, 1u);
  EXPECT_GT(sensor.getReadings().value, 0.0);
  EXPECT_NE(sensor.getReadings().value, valueBeforeReset);
}

TEST(ValueDeltaSensorTest, InvalidateForcesNewBaseline) {
  ValueDeltaSensor sensor{"eth0 download", SensorType::THROUGHPUT};

  sensor.setValue(1'000);
  sensor.updateValue();
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensor.setValue(2'000);
  sensor.updateValue();
  ASSERT_EQ(sensor.getReadings().times, 1u);

  sensor.invalidate();
  sensor.setValue(2'000);
  sensor.updateValue();
  EXPECT_EQ(sensor.getReadings().times, 1u); // still the sample from before invalidate

  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensor.setValue(4'000);
  sensor.updateValue();
  EXPECT_EQ(sensor.getReadings().times, 2u);
}

TEST(AddValueDeltaSensorTest, AppendsSensorAndReturnsBorrowedPointer) {
  std::vector<std::unique_ptr<Sensor>> sensors;

  ValueDeltaSensor *sensor =
      addValueDeltaSensor(sensors, "eth0 download", SensorType::THROUGHPUT);
  sensor->setValue(1'000);
  sensors.front()->updateValue();
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  sensor->setValue(2'000);
  sensors.front()->updateValue();

  ASSERT_EQ(sensors.size(), 1u);
  const nlohmann::json j = sensors.front()->serialize();
  EXPECT_EQ(j["name"], "eth0 download");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(SensorType::THROUGHPUT));
  EXPECT_GT(j["readings"]["value"].get<double>(), 0.0);
}
