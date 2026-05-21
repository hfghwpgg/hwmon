#include <csignal>
#include <filesystem>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <iostream>
#include "Runner.hpp"
#include "Device.hpp"

namespace fs = std::filesystem;

Runner::Runner() {
    Setup();
    //Run();
};

Runner::~Runner() {
    std::cout << "program finished" << std::endl;
}

void Runner::Setup() {
    const fs::path Path = "/sys/class/hwmon";
    for (auto &entry : fs::directory_iterator(Path)) {
        this->Devices.push_back(std::make_unique<Device>(entry.path()));
    }
};

void Runner::Run() {
    throw std::logic_error("unimplemented");
    // for (auto &dev : Devices) {

    // }
}