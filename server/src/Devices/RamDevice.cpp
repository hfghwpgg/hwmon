#include "RamDevice.hpp"

#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <unistd.h>

#include "DeviceType.hpp"
#include "Sensor/Sensor.hpp"
#include "Sensor/SensorType.hpp"
#include "Sensor/Source.hpp"
#include "Sensor/Transform.hpp"

RamDevice::RamDevice(std::filesystem::path meminfoPath) :
    Device("System RAM", DeviceType::RAM),
    meminfoPath(std::move(meminfoPath)) {}

void RamDevice::initialize() {
  if (!std::filesystem::exists(meminfoPath) || access(meminfoPath.c_str(), R_OK) == -1) {
    spdlog::error("{} inaccessible; skipping ram", meminfoPath.string());
    return;
  }
  totalSrc = Sensor::addPushSensor<TransformScale>(
      sensors, {"Total memory", SensorType::MEMORY, 1, false, false});
  availableSrc = Sensor::addPushSensor<TransformScale>(
      sensors, {"Used memory", SensorType::MEMORY, 1, true, true});
}

void RamDevice::read() {
  if (totalSrc == nullptr || availableSrc == nullptr)
    return;

  std::ifstream file(meminfoPath);
  if (!file.is_open()) {
    spdlog::error("{} suddenly inaccessible", meminfoPath.string());
    return;
  }

  bool gotTotal = false, gotAvailable = false;
  double totalValue = 0, availableValue = 0;
  std::string line, key;
  while (std::getline(file, line) && !(gotTotal && gotAvailable)) {
    double value;
    const auto colon = line.find(':');
    if (colon == std::string::npos)
      continue;
    key = line.substr(0, colon);
    if (key != "MemTotal" && key != "MemAvailable")
      continue;

    try {
      value = std::stod(line.substr(colon + 1));
    } catch (const std::exception &) {
      spdlog::error("failed to parse {} in {}", key, meminfoPath.string());
      continue;
    }
    // /proc/meminfo reports in KiB
    // so we multiply by 1024
    value *= 1024;
    if (key == "MemTotal") {
      totalValue = value;
      gotTotal = true;
    } else {
      availableValue = value;
      gotAvailable = true;
    }
  }

  if (!gotTotal || !gotAvailable || availableValue > totalValue) {
    spdlog::error("invalid or incomplete memory data in {}", meminfoPath.string());
    return;
  }

  totalSrc->setValue(totalValue);
  availableSrc->setValue(totalValue - availableValue);

  for (const auto &sensor : sensors) {
    sensor->updateValue();
  }
}

void RamDevice::resetReadings() {
  for (const auto &sensor : sensors) {
    sensor->resetReadings();
  }
}

nlohmann::json RamDevice::serialize() {
  nlohmann::json j;
  j["name"] = name;
  j["type"] = type;
  for (const auto &sensor : sensors) {
    j["sensors"].push_back(sensor->serialize());
  }
  return j;
}
