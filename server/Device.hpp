#pragma once
#include <memory>
#include <vector>
#include <filesystem>
#include <format>
#include "Sensor.hpp"
#include "SensorTypes.hpp"

namespace fs = std::filesystem;
using std::string;
using std::vector;
using std::unordered_map;
using std::format;

class Device {
public:
    Device(fs::path path, string name);
    Device(Device&&) noexcept = default;
    Device& operator=(Device&&) noexcept = default;
    Device(const Device&) noexcept = default;

    ~Device();
    SensorType DeduceSensorType(string parsedPart1);
    void createSensors(unordered_map<string, vector<string>> available_sensors);
    void Initialize();
    void Read();
    void Display();

private:
    fs::path path;
    string name;
    vector<std::unique_ptr<Sensor>> Sensors;
};