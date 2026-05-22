#pragma once
struct SensorReading {
    int value;
    float min_value;
    float max_value;
    double sum;
    unsigned int times;
};