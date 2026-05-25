#pragma once

#include <array>
#include <string_view>

// Edit this list to control which hwmon file suffixes are collected
// (e.g. temp1_input -> "input", fan2_label -> "label").
static inline constexpr std::array<std::string_view, 3> kSensorAttributeWhitelist = {
    "input",
    "label",
    "average",
};

inline bool isWhitelistedSensorAttribute(std::string_view attribute) {
  for (const auto allowed : kSensorAttributeWhitelist) {
    if (attribute == allowed)
      return true;
  }
  return false;
}
