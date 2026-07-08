#include <filesystem>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stddef.h>
#include <stdexcept>
#include <string>

#include "../Device.hpp"
#include "../helpers.hpp"
#include "GeneralDevice.hpp"
#include "SharedHwmonParser.hpp"

enum class DeviceType;

using std::string;

GeneralDevice::GeneralDevice(string name, DeviceType type, fs::path path) :
    Device(name, type),
    path(path) {
  spdlog::debug("CURRENT GENERAL DEVICE: {} <{}>", path.string(), name);
  if (helpers::pathType(path) != helpers::pathTypeEnum::DIRECTORY) {
    spdlog::critical("invalid path for device {}: {}", name, path.string());
    throw std::runtime_error("invalid path, check logs");
  };
}

#ifdef DEBUG
GeneralDevice::~GeneralDevice() {
  spdlog::debug("GeneralDevice destroyed: {}", name);
}
#endif

void GeneralDevice::initialize() {
  getName();
  const auto available_sensors = SharedHwmonParser::parseHwmonDirectory(path);
  SharedHwmonParser::createSensors(path, available_sensors, sensors);
}

// if hwmon contains name field, we use it
// as device name
void GeneralDevice::getName() {
  const auto namePath = path / "name";
  if (fs::exists(namePath) && access(namePath.c_str(), R_OK) != -1) {
    name = helpers::readFileFirstLine(namePath);
  }
}
