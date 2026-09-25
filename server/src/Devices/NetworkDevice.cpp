#include "NetworkDevice.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

#include "../DeviceType.hpp"
#include "Sensor/Sensor.hpp"
#include "Sensor/SensorType.hpp"
#include "Sensor/TransformDelta.hpp"

NetworkDevice::NetworkDevice(std::string name, std::filesystem::path statsPath) :
    Device(name, DeviceType::NETWORK),
    statsPath(statsPath) {}

void NetworkDevice::initialize() {
  Sensor::makeFileSensor<TransformDelta>(sensors, statsPath / "rx_bytes",
                                         {"Recieve speed", SensorType::THROUGHPUT});
  Sensor::makeFileSensor<TransformDelta>(sensors, statsPath / "tx_bytes",
                                         {"Transmit speed", SensorType::THROUGHPUT});
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
