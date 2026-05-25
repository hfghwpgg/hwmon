#pragma once
#include <string_view>
#include <unordered_map>

enum class SensorType {
  TEMPERATURE,
  FAN_SPEED,
  FREQUENCY,
  POWER,
  VOLTAGE,
  CURRENT,
  ENERGY,
  UNKNOWN,
};

constexpr float getDivider(SensorType type) {
  switch (type) {
  case SensorType::TEMPERATURE:
    return 1000.0f;
  case SensorType::VOLTAGE:
    return 1000.0f;
  case SensorType::FAN_SPEED:
    return 1.0f;
  case SensorType::FREQUENCY:
    return 1000000.0f;
  case SensorType::POWER:
    return 1000000.0f;
  case SensorType::CURRENT:
    return 1000.0f;
  case SensorType::ENERGY:
    return 1.0f;
  default:
    return 1.0f;
  }
}

static const std::unordered_map<std::string_view, SensorType> SensorConfigMap = {
    {"temp", SensorType::TEMPERATURE},
    {"in", SensorType::VOLTAGE},
    {"fan", SensorType::FAN_SPEED},
    {"power", SensorType::POWER},
    {"current", SensorType::CURRENT},
    {"voltage", SensorType::VOLTAGE},
    {"freq", SensorType::FREQUENCY},
    {"energy", SensorType::ENERGY} // energy is provided in ujoules, needs different
                                   // calculations
};