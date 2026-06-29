#include "CpuDevice.hpp"
#include "../Device.hpp"
#include "../DeviceType.hpp"
#include "Sensor.hpp"
#include "SensorType.hpp"
#include <cstdio>
#include <filesystem>
#include <format>
#include "spdlog/spdlog.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <variant>
#include <vector>
#include <print>
#include "helpers.hpp"


namespace fs = std::filesystem;
using std::string;

namespace {
const fs::path CPUFREQ_PATH = "/sys/devices/system/cpu/cpufreq/";
const fs::path CPUINFO_PATH = "/proc/cpuinfo";
const fs::path CPUUTIL_PATH = "/proc/stat";

std::vector<std::stringstream> utilSensorDescriptors;
} // namespace

// prob need to remove those params
// as this will be all managed by this
// class itself (same in Device)
CpuDevice::CpuDevice() :
    Device("cpu", DeviceType::CPU) {
  this->name = getCpuName();
}

#ifdef DEBUG
CpuDevice::~CpuDevice() {
  spdlog::debug("CPU destroyed");
}
#endif


void CpuDevice::getCpuCoreFrequency() {
  if (!fs::exists(CPUFREQ_PATH) || access(CPUFREQ_PATH.c_str(), R_OK) == -1) {
    spdlog::warn("{} inaccessible", CPUFREQ_PATH.string());
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
    spdlog::warn("{} inaccessible; setting general name for cpu", CPUINFO_PATH.string());
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
  return name;
}

// this is for cpu utilization
void CpuDevice::getCpuUtilization() {
  spdlog::critical("ive been called");
  if (!fs::exists(CPUUTIL_PATH) || access(CPUUTIL_PATH.c_str(), R_OK) == -1) {
    spdlog::warn("{} inaccessible", CPUUTIL_PATH.string());
    return;
  }

  std::ifstream stat(CPUUTIL_PATH);
  string line;
  while (std::getline(stat, line)) {
    line = helpers::trim(line);
    if (!line.starts_with("cpu")) {
      continue;
    }
    utilSensorDescriptors.push_back(std::stringstream());
  }
}

void CpuDevice::read() {
  spdlog::info("cpuUtil size is: {}", utilSensorDescriptors.size());
  // throw std::runtime_error("test");
  Device::read();
}

void CpuDevice::initialize() {
  this->getCpuCoreFrequency();
  this->getCpuUtilization();
}
