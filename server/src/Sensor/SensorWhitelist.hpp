#pragma once

#include <algorithm>
#include <array>
#include <string_view>

// Edit this list to control which hwmon file suffixes are collected
// (e.g. temp1_input -> "input", fan2_label -> "label").
static inline constexpr std::array<std::string_view, 3> sensorAttributeWhitelist = {
    "input",
    "label",
    "average",
};

inline bool IsWhitelistedSensorAttribute(std::string_view attribute) {
  return std::ranges::any_of(sensorAttributeWhitelist, [attribute](std::string_view allowed) {
    return attribute == allowed;
  });
}
