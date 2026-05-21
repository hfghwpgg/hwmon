#pragma once
#include <vector>
#include <memory>
#include "Device.hpp"

using std::vector;

class Runner {
public:
    Runner();
    ~Runner();
    void Setup();
    void Run();
private:
    vector<std::unique_ptr<Device>> Devices;
};