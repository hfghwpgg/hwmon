#include "NetworkDevice.hpp"

#include <array>
#include <format>
#include <map>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

#include "../DeviceType.hpp"
#include "../helpers.hpp"
#include "Sensor/SensorType.hpp"
#include "Sensor/TransformDelta.hpp"

// -------------------------------- <name, [rx, tx]>
using ifaceMap = std::map<std::string, std::array<unsigned long long, 2>>;

NetworkDevice::NetworkDevice(std::string name, std::filesystem::path netDev) :
    Device(name, DeviceType::NETWORK),
    netDevFD(netDev) {}

NetworkDevice::NetworkDevice(std::string name) :
    NetworkDevice(name, "/proc/net/dev") {}

NetworkDevice::~NetworkDevice() {
  if (netDevFD.is_open())
    netDevFD.close();
}

ifaceMap NetworkDevice::parseData() {
  netDevFD.clear();
  netDevFD.seekg(0);
  std::string line;
  // skip 2 first lines
  std::getline(netDevFD, line);
  std::getline(netDevFD, line);

  ifaceMap ifaces;

  while (std::getline(netDevFD, line)) {
    std::istringstream iss(line);
    std::string ifaceName;

    if (!std::getline(iss, ifaceName, ':'))
      continue;

    unsigned long long rx_bytes = 0;
    unsigned long long tx_bytes = 0;
    unsigned long long dummy = 0;

    iss >> rx_bytes;

    // we skip 7 columns
    for (int i = 0; i < 7; ++i) {
      iss >> dummy;
    }

    iss >> tx_bytes;

    ifaces.emplace(helpers::trim(ifaceName), std::array{rx_bytes, tx_bytes});
  }
  return ifaces;
}
void NetworkDevice::initialize() {
  auto ifaces = parseData();
  for (const auto &iface : ifaces) {
    Sources.emplace_back(Sensor::addPushSensor<TransformDelta>(
        sensors, {std::format("{} download", iface.first), SensorType::THROUGHPUT}));
    Sources.emplace_back(Sensor::addPushSensor<TransformDelta>(
        sensors, {std::format("{} upload", iface.first), SensorType::THROUGHPUT}));
  }
}

void NetworkDevice::read() {
  auto ifaces = parseData();
  short idx = 0;
  for (const auto &iface : ifaces) {
    const auto rx = iface.second[0];
    const auto tx = iface.second[1];
    Sources.at(idx++)->setValue(rx);
    Sources.at(idx++)->setValue(tx);
  }
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
