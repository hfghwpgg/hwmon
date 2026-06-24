#include "CpuDevice.hpp"
#include "../Device.hpp"
#include "../DeviceType.hpp"
#include "Sensor.hpp"
#include "SensorType.hpp"
#include <filesystem>
#include <format>
#include <spdlog/spdlog.h>
#include <unistd.h>

namespace fs = std::filesystem;
using std::string;

// prob need to remove those params
// as this will be all managed by this
// class itself (same in Device)
CpuDevice::CpuDevice() :
    Device("/sys/class/hwmon", "cpu", DeviceType::CPU) {
  getCpuCoreFrequency();
}

#ifdef DEBUG
CpuDevice::~CpuDevice() {
  spdlog::debug("CPU destroyed");
}
#endif

const fs::path CPUFREQ = "/sys/devices/system/cpu/cpufreq/";
const fs::path CPUINFO = "/proc/cpuinfo";
const fs::path CPUUTIL = "/proc/stat";

void CpuDevice::getCpuCoreFrequency() {
  if (!fs::exists(CPUFREQ) || access(CPUFREQ.c_str(), R_OK) == -1) {
    spdlog::warn("CPUFREQ unaccessible");
    return;
  }
  spdlog::critical("NOTE TO SELF: this interface returns values in kHz, not Hz");
  for (const auto &policy : fs::directory_iterator(CPUFREQ)) {
    const string filename = policy.path().stem().string();
    if (!filename.starts_with("policy"))
      continue;
    this->sensors.emplace_back(std::make_unique<Sensor>(
        // we know that file starts with policy, and thats 6 letters.
        // we only want core number
        policy.path() / "scaling_cur_freq", "CPU Core " + filename.substr(6),
        SensorType::FREQUENCY)); // divider is incorrect here
  }
}
