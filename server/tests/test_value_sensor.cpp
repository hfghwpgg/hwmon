#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

#include "Sensor.hpp"
#include "SensorType.hpp"
#include "ValueSensor.hpp"

TEST(ValueSensorTest, StaysNanUntilValueIsPushed) {
  ValueSensor sensor{"gpu_util", SensorType::UTILIZATION};

  sensor.updateValue();

  EXPECT_TRUE(std::isnan(sensor.getReadings().value));
  EXPECT_EQ(sensor.getReadings().times, 0u);
}

TEST(ValueSensorTest, AppliesNoDivider) {
  ValueSensor sensor{"edge", SensorType::TEMPERATURE};

  sensor.setValue(47.5);
  sensor.updateValue();

  EXPECT_DOUBLE_EQ(sensor.getReadings().value, 47.5);
  EXPECT_EQ(sensor.getReadings().times, 1u);
}

TEST(ValueSensorTest, TracksMinMaxAcrossUpdates) {
  ValueSensor sensor{"gpu_busy", SensorType::UTILIZATION};

  for (const double value : {10.0, 90.0, 50.0}) {
    sensor.setValue(value);
    sensor.updateValue();
  }

  const SensorReading readings = sensor.getReadings();
  EXPECT_DOUBLE_EQ(readings.min_value, 10.0);
  EXPECT_DOUBLE_EQ(readings.max_value, 90.0);
  EXPECT_EQ(readings.times, 3u);
}

// a backend that stops reporting must not keep its last value alive
TEST(ValueSensorTest, ConsumesValueOnce) {
  ValueSensor sensor{"power", SensorType::POWER};

  sensor.setValue(30.0);
  sensor.updateValue();
  sensor.updateValue();

  EXPECT_EQ(sensor.getReadings().times, 1u);
}

TEST(ValueSensorTest, ResetClearsAggregatesAndPendingValue) {
  ValueSensor sensor{"power", SensorType::POWER};

  sensor.setValue(30.0);
  sensor.updateValue();
  sensor.setValue(40.0);
  sensor.resetReadings();
  sensor.updateValue();

  EXPECT_EQ(sensor.getReadings().times, 0u);
  EXPECT_TRUE(std::isnan(sensor.getReadings().value));
}

TEST(AddValueSensorTest, AppendsSensorAndReturnsBorrowedPointer) {
  std::vector<std::unique_ptr<Sensor>> sensors;

  ValueSensor *sensor = addValueSensor(sensors, "vram_used", SensorType::MEMORY);
  sensor->setValue(1024);
  sensors.front()->updateValue();

  ASSERT_EQ(sensors.size(), 1u);
  const nlohmann::json j = sensors.front()->serialize();
  EXPECT_EQ(j["name"], "vram_used");
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(SensorType::MEMORY));
  EXPECT_DOUBLE_EQ(j["readings"]["value"].get<double>(), 1024.0);
}
