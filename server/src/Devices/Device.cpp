#include "Device.hpp"
#include "DeviceType.hpp"

#include <string>
#include <utility>

Device::Device(std::string name, DeviceType type) :
    name(std::move(name)),
    type(type) {
  sensors.reserve(10);
}

Device::~Device() = default;
