#include "Sensor.hpp"

Sensor::Sensor(
    std::string name,
    std::string nice_name,
    Type type,
    float value,
    float min_value,
    float max_value) : name(name),
                       nice_name(nice_name),
                       type(type),
                       value(value),
                       min_value(std::numeric_limits<float>::quiet_NaN()),
                       max_value(std::numeric_limits<float>::quiet_NaN())
{
}

void Sensor::updateValue(float value)
{
    this->value = value;
}