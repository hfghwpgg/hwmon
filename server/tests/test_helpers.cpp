#include <gtest/gtest.h>

#include "../Sensor.hpp"
#include "../SensorType.hpp"
#include <string>
#include <vector>

#include "../helpers.hpp"

TEST(RoundFloat, RoundsToRequestedPrecision) {
  EXPECT_NEAR(helpers::roundFloat(3.14159f, 2), 3.14f, 1e-4f);
  EXPECT_NEAR(helpers::roundFloat(123.456f, 1), 123.5f, 1e-4f);
  EXPECT_NEAR(helpers::roundFloat(10.0f, 0), 10.0f, 1e-4f);
}

TEST(RoundFloat, RoundsHalfAwayFromZero) {
  EXPECT_NEAR(helpers::roundFloat(2.5f, 0), 3.0f, 1e-4f);
  EXPECT_NEAR(helpers::roundFloat(-2.5f, 0), -3.0f, 1e-4f);
}

TEST(trim, ClearsTrailingSpaces) {
  std::string word1 = "     asd           ";
  std::string word2 = "          fsa";
  std::string word3 = "gfd               ";
  std::string word4 = "     |          dsa  |           ";
  EXPECT_EQ(helpers::trim(word1), "asd");
  EXPECT_EQ(helpers::trim(word2), "fsa");
  EXPECT_EQ(helpers::trim(word3), "gfd");
  EXPECT_EQ(helpers::trim(word4), "|          dsa  |");
}

TEST(deduceSensorType, ReturnsCorrectSensorType) {
  EXPECT_EQ(Sensor::deduceSensorType("temp1_input"), SensorType::TEMPERATURE);
  EXPECT_EQ(Sensor::deduceSensorType("in3_input"), SensorType::VOLTAGE);
  EXPECT_EQ(Sensor::deduceSensorType("voltage2_input"), SensorType::VOLTAGE);
  EXPECT_EQ(Sensor::deduceSensorType("fan20_input"), SensorType::FAN_SPEED);
  EXPECT_EQ(Sensor::deduceSensorType("power5_input"), SensorType::POWER);
  EXPECT_EQ(Sensor::deduceSensorType("current4_input"), SensorType::CURRENT);
  EXPECT_EQ(Sensor::deduceSensorType("freq1_input"), SensorType::FREQUENCY);
  EXPECT_EQ(Sensor::deduceSensorType("energy3_input"), SensorType::ENERGY);
  EXPECT_EQ(Sensor::deduceSensorType("util2_input"), SensorType::UTILIZATION);
  EXPECT_EQ(Sensor::deduceSensorType("unknown7_input"), SensorType::UNKNOWN);
}

TEST(pathType, ReturnsCorrectPathType) {
  EXPECT_EQ(helpers::pathType("/"), helpers::pathTypeEnum::DIRECTORY);
  EXPECT_EQ(helpers::pathType("/dev/null"), helpers::pathTypeEnum::FILE);
  EXPECT_EQ(helpers::pathType("/nonexistent/path"), helpers::pathTypeEnum::INVALID);
}
