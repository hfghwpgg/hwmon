#include "Runner.hpp"
#include "Device.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <print>
#include <thread>

namespace fs = std::filesystem;
using std::atomic;
using std::signal;

atomic<bool> Runner::running{true};

Runner::Runner() {
  Setup();
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
  signal(SIGINT, Stop);
  signal(SIGTERM, Stop);
  while (running.load(std::memory_order_relaxed)) {
    for (auto &d : Devices) {
      d.Display();
    }
    std::this_thread::sleep_for(std::chrono::seconds{1});
  }
}

void Runner::Stop(int sig) {
  std::print("Program interrupted, signal: {}\n", sig);
  running = false;
}
