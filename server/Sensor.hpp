#pragma once
#include <fstream>
#include <string>
#include <filesystem>
#include "SensorTypes.hpp"
#include "SensorReading.hpp"

class Sensor
{
public:
    Sensor(
        std::filesystem::path path,
        std::string name,
        SensorTypes type
    );
    ~Sensor();
    void updateValue();
    SensorReading getReadings();
    
    private:
    std::ifstream file;
    const std::string name;
    const SensorTypes type;
    SensorReading readings;
    
    int readFile();
};

