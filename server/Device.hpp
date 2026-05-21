#pragma once
#include <vector>
#include <filesystem>
#include "Sensor.hpp"

namespace fs = std::filesystem;

class Device {
public:
    Device(fs::path path);
    ~Device();
    void Initialize();
    void Read();

private:
    fs::path path;
    std::vector<Sensor> Sensors;
};