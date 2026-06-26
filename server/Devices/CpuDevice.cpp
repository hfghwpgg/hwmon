#include "CpuDevice.hpp"
#include "../Device.hpp"
#include "../DeviceType.hpp"
#include "Sensor.hpp"
#include "SensorType.hpp"
#include <filesystem>
#include <format>
#include <spdlog/spdlog.h>
#include <string>
#include <unistd.h>
#include <variant>
#include "helpers.hpp"

namespace fs = std::filesystem;
using std::string;

// prob need to remove those params
// as this will be all managed by this
// class itself (same in Device)
CpuDevice::CpuDevice() :
    Device("/sys/class/hwmon", "cpu", DeviceType::CPU) {
  getCpuCoreFrequency();
  this->name = getCpuName();
}

#ifdef DEBUG
CpuDevice::~CpuDevice() {
  spdlog::debug("CPU destroyed");
}
#endif

const fs::path CPUFREQ_PATH = "/sys/devices/system/cpu/cpufreq/";
const fs::path CPUINFO_PATH = "/proc/cpuinfo";
// const fs::path CPUUTIL_PATH = "/proc/stat";

void CpuDevice::getCpuCoreFrequency() {
  if (!fs::exists(CPUFREQ_PATH) || access(CPUFREQ_PATH.c_str(), R_OK) == -1) {
    spdlog::warn("CPUFREQ inaccessible");
    return;
  }
  spdlog::critical("NOTE TO SELF: this interface returns values in kHz, not Hz");
  for (const auto &policy : fs::directory_iterator(CPUFREQ_PATH)) {
    const string filename = policy.path().stem().string();
    if (!filename.starts_with("policy"))
      continue;
    this->sensors.emplace_back(std::make_unique<Sensor>(
        // we know that file starts with 'policy', and thats 6 letters.
        // we only want core number, so we substr the beginning
        policy.path() / "scaling_cur_freq", "CPU Core " + filename.substr(6) + " clock",
        SensorType::FREQUENCY)); // divider is incorrect here
  }
}

string CpuDevice::getCpuName() {
  std::string name = "cpumodel";
  if (!fs::exists(CPUINFO_PATH) || access(CPUINFO_PATH.c_str(), R_OK) == -1) {
    spdlog::warn("CPUINFO inaccessible; setting general name for cpu");
    return name;
  }
  std::ifstream file(CPUINFO_PATH);
  string line;
  while (std::getline(file, line)) {
    if (line.find("model name") == std::string::npos)
      continue;

    const auto colonIdx = line.find(':');
    if (colonIdx == std::string::npos)
      continue;

    line = line.substr(colonIdx + 1);
    name = helpers::trim(line);
    break;
  }
  file.close();
  if (name == "cpumodel")
    spdlog::error("couldn't find cpu name in CPUINFO");
  return name; // fallback
}
