#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "SensorReading.hpp"

TEST(SensorReadingSerialize, EmitsAllFields) {
  SensorReading reading{};
  reading.value = 1.5f;
  reading.min_value = 0.5f;
  reading.max_value = 2.5f;
  reading.sum = 10.0;
  reading.times = 4;

  const nlohmann::json j = reading.serialize();

  ASSERT_TRUE(j.contains("value"));
  ASSERT_TRUE(j.contains("min_value"));
  ASSERT_TRUE(j.contains("max_value"));
  ASSERT_TRUE(j.contains("sum"));
  ASSERT_TRUE(j.contains("times"));

  EXPECT_FLOAT_EQ(j["value"].get<float>(), 1.5f);
  EXPECT_FLOAT_EQ(j["min_value"].get<float>(), 0.5f);
  EXPECT_FLOAT_EQ(j["max_value"].get<float>(), 2.5f);
  EXPECT_DOUBLE_EQ(j["sum"].get<double>(), 10.0);
  EXPECT_EQ(j["times"].get<std::size_t>(), 4u);
}
