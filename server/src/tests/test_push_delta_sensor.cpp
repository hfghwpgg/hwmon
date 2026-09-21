#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "../Sensor/Sensor.hpp"
#include "../Sensor/SensorType.hpp"
#include "../Sensor/SourcePush.hpp"
#include "../Sensor/TransformDelta.hpp"

namespace {

// Pushed counter turned into a rate, e.g. /proc/net/dev byte counters.
struct PushDeltaFixture {
  // sensors must be declared before source: addPushSensor appends to the
  // vector and hands back a pointer into the sensor it created
  std::vector<std::unique_ptr<Sensor>> sensors;
  SourcePush *source;

  explicit PushDeltaFixture(std::string name, SensorType type = SensorType::THROUGHPUT) :
      source(Sensor::addPushSensor<TransformDelta>(sensors, {std::move(name), type})) {}

  Sensor &sensor() {
    return *sensors.front();
  }

  void push(double value) {
    source->setValue(value);
    sensor().updateValue();
  }
};

void tick() {
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
}

} // namespace

TEST(PushDeltaSensorTest, FirstPushedValueOnlyEstablishesBaseline) {
  PushDeltaFixture f{"eth0 download"};

  f.push(1'000);

  EXPECT_EQ(f.sensor().getReadings().times, 0u);
  EXPECT_TRUE(std::isnan(f.sensor().getReadings().value));
}

TEST(PushDeltaSensorTest, SecondReadReportsPositiveRate) {
  PushDeltaFixture f{"eth0 download"};

  f.push(1'000);
  tick();
  f.push(3'000);

  const SensorReading r = f.sensor().getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_TRUE(std::isfinite(r.value));
  EXPECT_GT(r.value, 0.0);
}

TEST(PushDeltaSensorTest, ReportsNegativeRateOnCounterReset) {
  PushDeltaFixture f{"eth0 download"};

  f.push(3'000);
  tick();
  f.push(1'000);

  const SensorReading r = f.sensor().getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_TRUE(std::isfinite(r.value));
  EXPECT_LT(r.value, 0.0);
}

TEST(PushDeltaSensorTest, StaysNanUntilValueIsPushed) {
  PushDeltaFixture f{"eth0 download"};

  f.sensor().updateValue();

  EXPECT_TRUE(std::isnan(f.sensor().getReadings().value));
  EXPECT_EQ(f.sensor().getReadings().times, 0u);
}

// a missing push must not wipe an already-established baseline
TEST(PushDeltaSensorTest, MissingPushDoesNotClobberBaseline) {
  PushDeltaFixture f{"eth0 download"};

  f.push(1'000);            // baseline
  f.sensor().updateValue(); // nothing pushed, source reports NotReady

  tick();
  f.push(2'000);

  const SensorReading r = f.sensor().getReadings();
  EXPECT_EQ(r.times, 1u);
  EXPECT_GT(r.value, 0.0);
}

TEST(PushDeltaSensorTest, ConsumesValueOnce) {
  PushDeltaFixture f{"eth0 download"};

  f.push(1'000);
  tick();
  f.push(2'000);
  ASSERT_EQ(f.sensor().getReadings().times, 1u);

  f.sensor().updateValue();
  EXPECT_EQ(f.sensor().getReadings().times, 1u);
}

TEST(PushDeltaSensorTest, ResetClearsAggregatesAndBaseline) {
  PushDeltaFixture f{"eth0 download"};

  f.push(1'000);
  tick();
  f.push(3'000);
  ASSERT_EQ(f.sensor().getReadings().times, 1u);
  const double valueBeforeReset = f.sensor().getReadings().value;

  f.sensor().resetReadings();
  const SensorReading cleared = f.sensor().getReadings();
  EXPECT_EQ(cleared.times, 0u);
  EXPECT_TRUE(std::isnan(cleared.value));
  EXPECT_TRUE(std::isnan(cleared.min_value));
  EXPECT_TRUE(std::isnan(cleared.max_value));
  EXPECT_DOUBLE_EQ(cleared.sum, 0.0);

  f.push(3'000); // re-establish baseline
  EXPECT_EQ(f.sensor().getReadings().times, 0u);

  tick();
  f.push(5'000);
  EXPECT_EQ(f.sensor().getReadings().times, 1u);
  EXPECT_GT(f.sensor().getReadings().value, 0.0);
  EXPECT_NE(f.sensor().getReadings().value, valueBeforeReset);
}

// invalidate() drops the pending push only; the rate baseline lives in the
// transform and is untouched, so the next push still yields a sample
TEST(PushDeltaSensorTest, InvalidateDropsPendingButKeepsBaseline) {
  PushDeltaFixture f{"eth0 download"};

  f.push(1'000);
  tick();
  f.push(2'000);
  ASSERT_EQ(f.sensor().getReadings().times, 1u);

  f.source->setValue(3'000);
  f.source->invalidate();
  f.sensor().updateValue();
  EXPECT_EQ(f.sensor().getReadings().times, 1u); // still the sample from before

  tick();
  f.push(4'000);
  EXPECT_EQ(f.sensor().getReadings().times, 2u);
}

TEST(AddPushDeltaSensorTest, AppendsSensorAndReturnsBorrowedPointer) {
  std::vector<std::unique_ptr<Sensor>> sensors;

  SourcePush *source = Sensor::addPushSensor<TransformDelta>(
      sensors, {"eth0 download", SensorType::THROUGHPUT});
  source->setValue(1'000);
  sensors.front()->updateValue();
  tick();
  source->setValue(2'000);
  sensors.front()->updateValue();

  ASSERT_EQ(sensors.size(), 1u);
  const nlohmann::json j = sensors.front()->serialize();
  EXPECT_EQ(j["name"], "eth0 download");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(SensorType::THROUGHPUT));
  EXPECT_GT(j["readings"]["value"].get<double>(), 0.0);
}
