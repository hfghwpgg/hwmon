#include "Device.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <stddef.h>
#include <spdlog/spdlog.h>
#include <fmt/base.h>
#include <memory>
#include <string>
#include <vector>
#include "Sensor.hpp"

enum class DeviceType;

using std::string;

Device::Device(string name, DeviceType type) :
    name(name),
    type(type),
    sensors() {
  sensors.reserve(10);
  spdlog::debug("CURRENT DEVICE: <{}>", name);
}

#ifdef DEBUG
Device::~Device() {
  spdlog::debug("Device destroyed: {}", name);
}
#endif

void Device::read() {
  for (auto &sensor : sensors) {
    sensor->updateValue();
  }
}

void Device::resetReadings() {
  for (auto &sensor : sensors) {
    sensor->resetReadings();
  }
}

nlohmann::json Device::serialize() {
  nlohmann::json j;
  j["name"] = name;
  j["type"] = type;
  for (auto &sensor : sensors) {
    j["sensors"] += sensor->serialize();
  }
  return j;
}
