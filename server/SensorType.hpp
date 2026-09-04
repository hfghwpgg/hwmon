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
  UTILIZATION,
  MEMORY,
  THROUGHPUT,
  UNKNOWN,
};

constexpr unsigned int getDivider(SensorType type) {
  switch (type) {
  case SensorType::TEMPERATURE:
    return 1'000;
  case SensorType::VOLTAGE:
    return 1'000;
  case SensorType::FAN_SPEED:
    return 1;
  case SensorType::FREQUENCY:
    return 1'000'000;
  case SensorType::POWER:
    return 1'000'000;
  case SensorType::CURRENT:
    return 1'000;
  case SensorType::ENERGY:
    return 1;
  case SensorType::UTILIZATION:
    return 1;
  case SensorType::MEMORY: // bytes
    return 1;
  case SensorType::THROUGHPUT: // bytes per second
    return 1;
  default:
    return 1;
  }
}

static const std::unordered_map<std::string_view, SensorType> sensorConfigMap = {
    {"temp", SensorType::TEMPERATURE},
    {"in", SensorType::VOLTAGE},
    {"voltage", SensorType::VOLTAGE},
    {"fan", SensorType::FAN_SPEED},
    {"power", SensorType::POWER},
    {"current", SensorType::CURRENT},
    {"freq", SensorType::FREQUENCY},
    {"energy", SensorType::ENERGY}, // energy is provided in ujoules, needs different
                                    // calculations

    {"util", SensorType::UTILIZATION} // idk if its correct
};
