#include <gtest/gtest.h>

#include "SensorType.hpp"

TEST(GetDivider, KnownTypesHaveExpectedDividers) {
  EXPECT_EQ(getDivider(SensorType::TEMPERATURE), 1000);
  EXPECT_EQ(getDivider(SensorType::VOLTAGE), 1000);
  EXPECT_EQ(getDivider(SensorType::CURRENT), 1000);
  EXPECT_EQ(getDivider(SensorType::FREQUENCY), 1000000);
  EXPECT_EQ(getDivider(SensorType::POWER), 1000000);
  EXPECT_EQ(getDivider(SensorType::FAN_SPEED), 1);
  EXPECT_EQ(getDivider(SensorType::ENERGY), 1);
  EXPECT_EQ(getDivider(SensorType::UTILIZATION), 1);
  EXPECT_EQ(getDivider(SensorType::MEMORY), 1);
  EXPECT_EQ(getDivider(SensorType::THROUGHPUT), 1);
}

TEST(GetDivider, UnknownFallsBackToOne) {
  EXPECT_EQ(getDivider(SensorType::UNKNOWN), 1);
}

TEST(GetDivider, IsConstexpr) {
  static_assert(getDivider(SensorType::TEMPERATURE) == 1000);
  static_assert(getDivider(SensorType::FAN_SPEED) == 1);
  SUCCEED();
}

TEST(SensorConfigMap, MapsPrefixesToTypes) {
  EXPECT_EQ(sensorConfigMap.at("temp"), SensorType::TEMPERATURE);
  EXPECT_EQ(sensorConfigMap.at("in"), SensorType::VOLTAGE);
  EXPECT_EQ(sensorConfigMap.at("voltage"), SensorType::VOLTAGE);
  EXPECT_EQ(sensorConfigMap.at("fan"), SensorType::FAN_SPEED);
  EXPECT_EQ(sensorConfigMap.at("power"), SensorType::POWER);
  EXPECT_EQ(sensorConfigMap.at("current"), SensorType::CURRENT);
  EXPECT_EQ(sensorConfigMap.at("freq"), SensorType::FREQUENCY);
  EXPECT_EQ(sensorConfigMap.at("energy"), SensorType::ENERGY);
}

TEST(SensorConfigMap, UnknownPrefixIsAbsent) {
  EXPECT_EQ(sensorConfigMap.find("bogus"), sensorConfigMap.end());
}
