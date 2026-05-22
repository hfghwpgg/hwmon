#pragma once
#include <string_view>
#include <unordered_map>

enum SensorType
{
    TEMPERATURE = 1000,
    FAN_SPEED = 1,
    FREQUENCY = 1000000,
    POWER = 1000000,
    VOLTAGE = 1000,
    CURRENT = 1000,
    ENERGY = 1,
    UNKNOWN = 1
};

static const std::unordered_map<std::string_view, SensorType> SensorConfigMap = {
    {"temp", SensorType::TEMPERATURE},
    {"in", SensorType::VOLTAGE},
    {"fan", SensorType::FAN_SPEED},
    {"power", SensorType::POWER},
    {"current", SensorType::CURRENT},
    {"voltage", SensorType::VOLTAGE},
    {"freq", SensorType::FREQUENCY},
    {"energy", SensorType::ENERGY} // energy is provided in Joules, needs different calculations
};