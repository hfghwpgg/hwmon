#include <filesystem>
#include <chrono>
#include <thread>
#include <iostream>
#include "Runner.hpp"
#include "Device.hpp"

namespace fs = std::filesystem;

Runner::Runner() {
    Setup();
    Run();
};

Runner::~Runner() {
    std::cout << "runner stopped" << std::endl;
}

void Runner::Setup() {
    const fs::path Path = "/sys/class/hwmon";
    for (auto &entry : fs::directory_iterator(Path)) {
        // auto d = new Device(entry.path(), entry.path().stem());
        this->Devices.emplace_back(entry.path(), entry.path().filename());
    }
};

void Runner::Run() {
    while(1) {
        for (auto &d : Devices) {
            d.Display();
        }
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }
}