#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "roundFloat.hpp"
#include "vectorFind.hpp"

TEST(RoundFloat, RoundsToRequestedPrecision) {
  EXPECT_NEAR(roundFloat(3.14159f, 2), 3.14f, 1e-4f);
  EXPECT_NEAR(roundFloat(123.456f, 1), 123.5f, 1e-4f);
  EXPECT_NEAR(roundFloat(10.0f, 0), 10.0f, 1e-4f);
}

TEST(RoundFloat, RoundsHalfAwayFromZero) {
  EXPECT_NEAR(roundFloat(2.5f, 0), 3.0f, 1e-4f);
  EXPECT_NEAR(roundFloat(-2.5f, 0), -3.0f, 1e-4f);
}

TEST(IsInVector, FindsExistingElements) {
  const std::vector<int> nums{1, 2, 3, 42};
  EXPECT_TRUE(isInVector(nums, 1));
  EXPECT_TRUE(isInVector(nums, 42));
}

TEST(IsInVector, ReturnsFalseForMissingElements) {
  const std::vector<int> nums{1, 2, 3};
  EXPECT_FALSE(isInVector(nums, 99));

  const std::vector<int> empty{};
  EXPECT_FALSE(isInVector(empty, 1));
}

TEST(IsInVector, WorksWithStrings) {
  const std::vector<std::string> words{"input", "label"};
  EXPECT_TRUE(isInVector<std::string>(words, "label"));
  EXPECT_FALSE(isInVector<std::string>(words, "average"));
}
