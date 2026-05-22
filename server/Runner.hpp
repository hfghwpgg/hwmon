#pragma once
#include <vector>
#include "Device.hpp"

using std::vector;

class Runner {
public:
    Runner();
    ~Runner();
    void Setup();
    void Run();
private:
    vector<Device> Devices;
};