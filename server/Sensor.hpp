#pragma once
#include <fstream>
#include <filesystem>
#include "SensorTypes.hpp"
#include "SensorReading.hpp"

namespace fs = std::filesystem; 
using std::string;
using std::ifstream;

class Sensor {
public:
    Sensor(
        fs::path path,
        string name,
        SensorType type
    );
    
    //allow moving
    Sensor(Sensor&&) noexcept = default;
    Sensor& operator=(Sensor&&) noexcept = default;

    //block copying
    Sensor(const Sensor&) = delete;
    Sensor& operator=(const Sensor&) = delete;

    ~Sensor();
    void updateValue();
    SensorReading getReadings();
    
    
protected:
    ifstream file;
    string name;
    const SensorType type;
    SensorReading readings;
    
    float readAndPrepareValue();
};

