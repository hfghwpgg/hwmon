#include <iostream>
#include <string>
#include <memory>
// #include "discover.hpp"
#include "Device.hpp"

int main()
{
    std::cout << "hello world:" << std::endl;
    // test();
    auto d = std::make_unique<Device>("/sys/class/hwmon/hwmon3");

    return 0;
}