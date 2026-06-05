#include "Runner.hpp"
#include "Device.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <print>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;
using std::println;

Runner::Runner(SharedState &state) :
    state(state) {
  devices.reserve(10);
  Setup();
};

Runner::~Runner() {
  println("runner stopped");
}

void Runner::Setup() {
  const fs::path Path = "/sys/class/hwmon";
  for (auto &entry : fs::directory_iterator(Path)) {
    this->devices.emplace_back(entry.path(), entry.path().filename());
  }
};

void Runner::Run() {
  while (state.running.load(std::memory_order_relaxed)) {
    json serializedDevices = json::array();
    for (auto &device : devices) {
      device.Read();
      serializedDevices.push_back(device.Serialize());
    }

    // Publish the latest snapshot for clients to pull on request.
    state.snapshot.store(std::make_shared<const std::string>(serializedDevices.dump()),
                         std::memory_order_release);

    const unsigned int intervalMs = state.intervalMs.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds{intervalMs});
  }
}
