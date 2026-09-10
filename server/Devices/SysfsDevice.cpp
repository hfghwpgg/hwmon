#include <filesystem>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stddef.h>
#include <stdexcept>
#include <string>

#include "../Device.hpp"
#include "../helpers.hpp"
#include "SharedHwmonParser.hpp"
#include "SysfsDevice.hpp"

enum class DeviceType;

using std::string;

SysfsDevice::SysfsDevice(string name, DeviceType type, fs::path path) :
    Device(name, type),
    path(path),
    sensors() {
  sensors.reserve(10);
  spdlog::trace("CURRENT SYSFS DEVICE: {} <{}>", path.string(), name);
  if (helpers::pathType(path) != helpers::pathTypeEnum::DIRECTORY) {
    spdlog::critical("invalid path for device {}: {}", name, path.string());
    throw std::runtime_error("invalid path, check logs");
  };
}

#ifdef DEBUG
SysfsDevice::~SysfsDevice() {
  spdlog::trace("GeneralDevice destroyed: {}", name);
}
#endif

void SysfsDevice::initialize() {
  getName();
  const auto available_sensors = SharedHwmonParser::parseHwmonDirectory(path);
  sensors = SharedHwmonParser::createSensors(path, available_sensors);
}

void SysfsDevice::read() {
  for (const auto &sensor : sensors) {
    sensor->updateValue();
  }
}

void SysfsDevice::resetReadings() {
  for (const auto &sensor : sensors) {
    sensor->resetReadings();
  }
}

nlohmann::json SysfsDevice::serialize() {
  nlohmann::json j;
  j["name"] = name;
  j["type"] = type;
  for (auto &sensor : sensors) {
    j["sensors"] += sensor->serialize();
  }
  return j;
}

// if hwmon contains name field, we use it
// as device name
void SysfsDevice::getName() {
  const auto namePath = path / "name";
  if (fs::exists(namePath) && access(namePath.c_str(), R_OK) != -1) {
    name = helpers::readFileFirstLine(namePath);
  }
}
