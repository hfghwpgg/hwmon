#include "Device.hpp"
#include "DeviceType.hpp"

#include <string>

Device::Device(std::string name, DeviceType type) :
    name(name),
    type(type) {
  sensors.reserve(10);
}

Device::~Device() = default;
