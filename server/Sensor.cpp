#include <cmath>
#include <iostream>
#include <ostream>
#include <string>
#include <format>
#include "Sensor.hpp"
#include "SensorReading.hpp"
#include "SensorTypes.hpp"


Sensor::Sensor(fs::path path,string name, SensorType type) :
        file(path),
        name(name),
        type(type),
        readings{0, NAN, NAN, 0, 0}
        {
            std::cout << "sensor init: " << path << std::endl;
            if (!file.is_open()) {
                std::cout << "unable to open file: " << path << std::endl;
                throw;
            };
        }

Sensor::~Sensor() {
    file.close();
    std::cout << "sensor destroyed: "<< this->name << std::endl;
}

float Sensor::readAndPrepareValue() {
    file.seekg(0);
    std::string str;
    std::getline(file, str);
    
    return std::stoi(str);
}
void Sensor::updateValue() {
    float value = readAndPrepareValue();
    if (std::isnan(value)) return;
    this->readings.value = value;
    this->readings.sum += value;
    this->readings.times++;

    if (this->readings.min_value > value || std::isnan(this->readings.min_value)) {
        this->readings.min_value = value;
    }
    if (this->readings.max_value < value || std::isnan(this->readings.max_value)) {
        this->readings.max_value = value;
    }
}

SensorReading Sensor::getReadings() {
    return this->readings;
}