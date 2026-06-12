#pragma once
#include "Sensor.hpp"
#include "SensorType.hpp"
#include <chrono>
#include <filesystem>
#include <string>

enum class SensorType;

class EnergySensor : public Sensor {
public:
  EnergySensor(std::filesystem::path path, std::string name, SensorType type);

private:
  float PrepareValue() override;

private:
  long double lastReading;
  std::chrono::steady_clock::time_point lastTime;
};
