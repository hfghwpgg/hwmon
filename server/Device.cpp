#include "Device.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stddef.h>
#include <string>

Device::Device(std::string name, DeviceType type) :
    name(name),
    type(type) {}

Device::~Device() = default;
