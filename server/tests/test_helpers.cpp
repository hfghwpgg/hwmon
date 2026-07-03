#include <gtest/gtest.h>

#include <string>
#include <vector>
#include "SensorType.hpp"

#include "helpers.hpp"

TEST(RoundFloat, RoundsToRequestedPrecision) {
  EXPECT_NEAR(helpers::roundFloat(3.14159f, 2), 3.14f, 1e-4f);
  EXPECT_NEAR(helpers::roundFloat(123.456f, 1), 123.5f, 1e-4f);
  EXPECT_NEAR(helpers::roundFloat(10.0f, 0), 10.0f, 1e-4f);
}

TEST(RoundFloat, RoundsHalfAwayFromZero) {
  EXPECT_NEAR(helpers::roundFloat(2.5f, 0), 3.0f, 1e-4f);
  EXPECT_NEAR(helpers::roundFloat(-2.5f, 0), -3.0f, 1e-4f);
}

TEST(IsInVector, FindsExistingElements) {
  const std::vector<int> nums{1, 2, 3, 42};
  EXPECT_TRUE(helpers::isInVector(nums, 1));
  EXPECT_TRUE(helpers::isInVector(nums, 42));
}

TEST(IsInVector, ReturnsFalseForMissingElements) {
  const std::vector<int> nums{1, 2, 3};
  EXPECT_FALSE(helpers::isInVector(nums, 99));

  const std::vector<int> empty{};
  EXPECT_FALSE(helpers::isInVector(empty, 1));
}

TEST(IsInVector, WorksWithStrings) {
  const std::vector<std::string> words{"input", "label"};
  EXPECT_TRUE(helpers::isInVector<std::string>(words, "label"));
  EXPECT_FALSE(helpers::isInVector<std::string>(words, "average"));
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
  EXPECT_EQ(helpers::deduceSensorType("temp1_input"), SensorType::TEMPERATURE);
  EXPECT_EQ(helpers::deduceSensorType("in3_input"), SensorType::VOLTAGE);
  EXPECT_EQ(helpers::deduceSensorType("voltage2_input"), SensorType::VOLTAGE);
  EXPECT_EQ(helpers::deduceSensorType("fan2_input"), SensorType::FAN_SPEED);
  EXPECT_EQ(helpers::deduceSensorType("power5_input"), SensorType::POWER);
  EXPECT_EQ(helpers::deduceSensorType("curr4_input"), SensorType::CURRENT);
  EXPECT_EQ(helpers::deduceSensorType("freq1_input"), SensorType::FREQUENCY);
  EXPECT_EQ(helpers::deduceSensorType("energy3_input"), SensorType::ENERGY);
  EXPECT_EQ(helpers::deduceSensorType("util2_input"), SensorType::UTILIZATION);
  EXPECT_EQ(helpers::deduceSensorType("unknown7_input"), SensorType::UNKNOWN);
}

TEST(pathType, ReturnsCorrectPathType) {
  EXPECT_EQ(helpers::pathType("/"), helpers::pathTypeEnum::DIRECTORY);
  EXPECT_EQ(helpers::pathType("/dev/null"), helpers::pathTypeEnum::FILE);
  EXPECT_EQ(helpers::pathType("/nonexistent/path"), helpers::pathTypeEnum::INVALID);
}
