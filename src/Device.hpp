#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "Sensor.hpp"

namespace fs = std::filesystem;

class Device
{
public:
    Device(fs::path path);
    void Initialize();

private:
    fs::path path;
    std::vector<Sensor> Sensors;
};