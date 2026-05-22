#pragma once
#include <chrono>
#include "Sensor.hpp"
#include "SensorTypes.hpp"

class EnergySensor : public Sensor {
public:
    EnergySensor(
        fs::path path,
        string name,
        SensorType type
    );
    float readAndPrepareValue();
private:
    long double lastReading;
    std::chrono::steady_clock::time_point lastTime;
};