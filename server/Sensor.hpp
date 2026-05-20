#pragma once
#include <string>
#include <limits>
#include "Sensor_types.hpp"

class Sensor
{
public:
    Sensor(
        std::string name,
        std::string nice_name,
        Type type,
        float value,
        float min_value,
        float max_value);
    void updateValue(float value);

private:
    const std::string name;
    const std::string nice_name;
    const Type type;
    float value;
    const float min_value;
    const float max_value;
};