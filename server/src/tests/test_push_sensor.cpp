#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

#include "../Sensor/Sensor.hpp"
#include "../Sensor/SensorType.hpp"
#include "../Sensor/SourcePush.hpp"
#include "../Sensor/TransformScale.hpp"

namespace {

// Sensor fed by its owner (NVML, ROCm SMI, i915 PMU) rather than by a file.
// The SourcePush is owned by the Sensor, so tests keep a borrowed pointer to
// push values the same way a device does.
struct PushSensorFixture {
  // sensors must outlive and be declared before source: addPushSensor appends
  // to the vector and hands back a pointer into the sensor it created
  std::vector<std::unique_ptr<Sensor>> sensors;
  SourcePush *source;

  PushSensorFixture(std::string name, SensorType type, unsigned int rawDivider = 1) :
      source(Sensor::addPushSensor<TransformScale>(sensors, {std::move(name), type, rawDivider})) {}

  Sensor &sensor() {
    return *sensors.front();
  }
};

} // namespace

TEST(PushSensorTest, StaysNanUntilValueIsPushed) {
  PushSensorFixture f{"gpu_util", SensorType::UTILIZATION};

  f.sensor().updateValue();

  EXPECT_TRUE(std::isnan(f.sensor().getReadings().value));
  EXPECT_EQ(f.sensor().getReadings().times, 0u);
}

TEST(PushSensorTest, AppliesNoDivider) {
  PushSensorFixture f{"edge", SensorType::TEMPERATURE};

  f.source->setValue(47.5);
  f.sensor().updateValue();

  EXPECT_DOUBLE_EQ(f.sensor().getReadings().value, 47.5);
  EXPECT_EQ(f.sensor().getReadings().times, 1u);
}

TEST(PushSensorTest, TracksMinMaxAcrossUpdates) {
  PushSensorFixture f{"gpu_busy", SensorType::UTILIZATION};

  for (const double value : {10.0, 90.0, 50.0}) {
    f.source->setValue(value);
    f.sensor().updateValue();
  }

  const SensorReading readings = f.sensor().getReadings();
  EXPECT_DOUBLE_EQ(readings.min_value, 10.0);
  EXPECT_DOUBLE_EQ(readings.max_value, 90.0);
  EXPECT_EQ(readings.times, 3u);
}

// a backend that stops reporting must not keep its last value alive
TEST(PushSensorTest, ConsumesValueOnce) {
  PushSensorFixture f{"power", SensorType::POWER};

  f.source->setValue(30.0);
  f.sensor().updateValue();
  f.sensor().updateValue();

  EXPECT_EQ(f.sensor().getReadings().times, 1u);
}

TEST(PushSensorTest, InvalidateDropsPendingValue) {
  PushSensorFixture f{"power", SensorType::POWER};

  f.source->setValue(30.0);
  f.source->invalidate();
  f.sensor().updateValue();

  EXPECT_EQ(f.sensor().getReadings().times, 0u);
  EXPECT_TRUE(std::isnan(f.sensor().getReadings().value));
}

TEST(PushSensorTest, ResetClearsAggregatesAndPendingValue) {
  PushSensorFixture f{"power", SensorType::POWER};

  f.source->setValue(30.0);
  f.sensor().updateValue();
  f.source->setValue(40.0);
  f.sensor().resetReadings();
  f.sensor().updateValue();

  // a value pushed before the reset belongs to the previous window
  EXPECT_EQ(f.sensor().getReadings().times, 0u);
  EXPECT_TRUE(std::isnan(f.sensor().getReadings().value));
}

TEST(PushSensorTest, NonAggregatingSensorKeepsOnlyLatestSample) {
  std::vector<std::unique_ptr<Sensor>> sensors;
  SourcePush *source = Sensor::addPushSensor<TransformScale>(
      sensors, {"instant", SensorType::UTILIZATION, 1, /*aggregateData=*/false});

  for (const double value : {10.0, 90.0}) {
    source->setValue(value);
    sensors.front()->updateValue();
  }

  const SensorReading readings = sensors.front()->getReadings();
  EXPECT_DOUBLE_EQ(readings.sum, 90.0);
  EXPECT_EQ(readings.times, 1u);
}

TEST(AddPushSensorTest, AppendsSensorAndReturnsBorrowedPointer) {
  std::vector<std::unique_ptr<Sensor>> sensors;

  SourcePush *source =
      Sensor::addPushSensor<TransformScale>(sensors, {"vram_used", SensorType::MEMORY});
  source->setValue(1024);
  sensors.front()->updateValue();

  ASSERT_EQ(sensors.size(), 1u);
  const nlohmann::json j = sensors.front()->serialize();
  EXPECT_EQ(j["name"], "vram_used");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(SensorType::MEMORY));
  EXPECT_DOUBLE_EQ(j["readings"]["value"].get<double>(), 1024.0);
}

TEST(AddPushSensorTest, MarksPrimarySensor) {
  std::vector<std::unique_ptr<Sensor>> sensors;

  Sensor::addPushSensor<TransformScale>(
      sensors, {"CPU", SensorType::UTILIZATION, 1, /*aggregateData=*/true, /*isPrimary=*/true});

  EXPECT_TRUE(sensors.front()->serialize()["isPrimary"].get<bool>());
}
