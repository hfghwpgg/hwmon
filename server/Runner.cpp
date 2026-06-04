#include "Runner.hpp"
#include "Device.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <nlohmann/json_fwd.hpp>
#include <print>
#include <ratio>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using std::atomic;
using std::signal;

atomic<bool> Runner::running{true};

Runner::Runner(unsigned int intervalMs) :
    intervalMs(intervalMs) {
  devices.reserve(10);
  Setup();
};

Runner::~Runner() {
  std::cout << "runner stopped" << std::endl;
}

void Runner::Setup() {
  const fs::path Path = "/sys/class/hwmon";
  for (auto &entry : fs::directory_iterator(Path)) {
    // auto d = new Device(entry.path(), entry.path().stem());
    this->devices.emplace_back(entry.path(), entry.path().filename());
  }
};

void Runner::Run() {
  signal(SIGINT, Interrupt);
  signal(SIGTERM, Interrupt);
  while (running.load(std::memory_order_relaxed)) {
    std::vector<nlohmann::json> serializedDevices;
    serializedDevices.reserve(10);
    for (auto &device : devices) {
      // doing device.Read() and then serializing
      // iterates through all sensors twice, but in the
      // future i plan on NOT serializing data if its
      // not needed (no client is connected to read data)
      device.Read();
      serializedDevices.push_back(device.Serialize());
    }
    std::cout << serializedDevices << '\n';
    std::this_thread::sleep_for(std::chrono::milliseconds{intervalMs});
  }
}

void Runner::Interrupt(int sig) {
  std::print("Program interrupted, signal: {}\n", sig);
  running = false;
}
