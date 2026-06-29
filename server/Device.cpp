#include "Device.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <stddef.h>
#include <spdlog/spdlog.h>
#include <fmt/base.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <utility>

#include "Sensor.hpp"

enum class DeviceType;

using std::string;

Device::Device(string name, DeviceType type) :
    name(name),
    type(type),
    sensors() {
  sensors.reserve(10);
  spdlog::debug("CURRENT DEVICE: <{}>", this->name);
}

#ifdef DEBUG
Device::~Device() {
  spdlog::debug("Device destroyed: {}", this->name);
}
#endif

void Device::read() {
  for (auto &sensor : sensors) {
    sensor->updateValue();
  }
}

nlohmann::json Device::serialize() {
  nlohmann::json j;
  j["name"] = this->name;
  j["type"] = this->type;
  for (auto &sensor : sensors) {
    j["sensors"] += sensor->serialize();
  }
  return j;
}
