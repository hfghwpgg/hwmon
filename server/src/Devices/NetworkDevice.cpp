#include "NetworkDevice.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

#include "DeviceType.hpp"
#include "Sensor/Sensor.hpp"
#include "Sensor/SensorType.hpp"
#include "Sensor/Transform.hpp"

NetworkDevice::NetworkDevice(std::string name, std::filesystem::path statsPath) :
    Device(std::move(name), DeviceType::NETWORK),
    statsPath(std::move(statsPath)) {}

void NetworkDevice::initialize() {
  Sensor::makeFileSensor<DeltaTransform>(sensors, statsPath / "rx_bytes",
                                         {.name = "Recieve speed", .type = SensorType::THROUGHPUT});
  Sensor::makeFileSensor<DeltaTransform>(
      sensors, statsPath / "tx_bytes", {.name = "Transmit speed", .type = SensorType::THROUGHPUT});
}

void NetworkDevice::read() {
  for (const auto &sensor : sensors) {
    sensor->updateValue();
  }
}

void NetworkDevice::resetReadings() {
  for (const auto &sensor : sensors) {
    sensor->resetReadings();
  }
}

nlohmann::json NetworkDevice::serialize() {
  nlohmann::json j;
  j["name"] = name;
  j["type"] = type;
  for (const auto &sensor : sensors) {
    j["sensors"].push_back(sensor->serialize());
  }
  return j;
}
