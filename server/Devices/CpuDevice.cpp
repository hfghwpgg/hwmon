#include "CpuDevice.hpp"
#include "../Device.hpp"
#include "../DeviceType.hpp"
#include "../Sensor.hpp"
#include "../SensorType.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include "spdlog/spdlog.h"
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>
#include "helpers.hpp"

namespace fs = std::filesystem;

namespace {
const fs::path CPUFREQ_PATH = "/sys/devices/system/cpu/cpufreq/";
const fs::path CPUINFO_PATH = "/proc/cpuinfo";
const fs::path CPUUTIL_PATH = "/proc/stat";
} // namespace

// prob need to remove those params
// as this will be all managed by this
// class itself (same in Device)
CpuDevice::CpuDevice() :
    Device("cpu", DeviceType::CPU) {
  name = getCpuName();
}

CpuDevice::~CpuDevice() {
  if (CPUUTIL_FD.is_open())
    CPUUTIL_FD.close();
  spdlog::debug("CPU destroyed");
}


// this interface returns frequency in kHz, not Hz.
void CpuDevice::getCpuCoreFrequency() {
  if (!fs::exists(CPUFREQ_PATH) || access(CPUFREQ_PATH.c_str(), R_OK) == -1) {
    spdlog::error("{} inaccessible", CPUFREQ_PATH.string());
    return;
  }
  for (const auto &policy : fs::directory_iterator(CPUFREQ_PATH)) {
    const std::string filename = policy.path().stem().string();
    if (!filename.starts_with("policy"))
      continue;

    auto fd = std::make_unique<std::ifstream>(policy.path() / "scaling_cur_freq");
    sensors.emplace_back(std::make_unique<Sensor>(
        // we know that file starts with 'policy', and thats 6 letters.
        // we only want core number, so we substr the beginning
        std::move(fd), "cpu" + filename.substr(6), SensorType::FREQUENCY, 1000));
  }
}

std::string CpuDevice::getCpuName() {
  std::string name = "cpumodel";
  if (!fs::exists(CPUINFO_PATH) || access(CPUINFO_PATH.c_str(), R_OK) == -1) {
    spdlog::error("{} inaccessible; setting general name for cpu", CPUINFO_PATH.string());
    return name;
  }
  std::ifstream CPUINFO_FD(CPUINFO_PATH);
  std::string line;
  while (std::getline(CPUINFO_FD, line)) {
    if (line.find("model name") == std::string::npos)
      continue;

    const auto colonIdx = line.find(':');
    if (colonIdx == std::string::npos)
      continue;

    line = line.substr(colonIdx + 1);
    name = helpers::trim(line);
    break;
  }
  CPUINFO_FD.close();
  if (name == "cpumodel")
    spdlog::error("couldn't find cpu name in CPUINFO");
  return name;
}

// this is just creating right amount of
// stringstreams for whole cpu + each core
void CpuDevice::initCpuUtilization() {
  if (!fs::exists(CPUUTIL_PATH) || access(CPUUTIL_PATH.c_str(), R_OK) == -1) {
    spdlog::error("{} inaccessible", CPUUTIL_PATH.string());
    return;
  }

  CPUUTIL_FD.open(CPUUTIL_PATH);
  std::string line;
  while (std::getline(CPUUTIL_FD, line)) {
    line = helpers::trim(line);
    if (!line.starts_with("cpu")) {
      continue;
    }
    std::stringstream ss(line);
    std::string cpuCoreNum;
    ss >> cpuCoreNum;
    utilSensors.emplace(cpuCoreNum,
                        utilSensorData{std::make_shared<std::stringstream>(), {0, 0, false}});
    sensors.emplace_back(std::make_unique<Sensor>(utilSensors.at(cpuCoreNum).dataStream, cpuCoreNum,
                                                  SensorType::UTILIZATION));
  }
}

// actually reading stuff
void CpuDevice::readCpuUtilization() {
  if (utilSensors.size() == 0) {
    spdlog::critical("no cpu utilization detected");
    return;
  }
  if (!CPUUTIL_FD.is_open()) {
    spdlog::critical("access to /proc/stat suddenly lost");
    return;
  }

  CPUUTIL_FD.clear();
  CPUUTIL_FD.seekg(0);

  std::string line;
  while (std::getline(CPUUTIL_FD, line)) {
    if (!line.starts_with("cpu"))
      continue;

    std::stringstream ss(line);
    std::string cpuCoreNum;
    ss >> cpuCoreNum; // cpu or cpuN

    unsigned long long number;        // placeholder for readings
    unsigned long long totalTime = 0; // cpu time
    unsigned long long idleTime = 0;  // cpu time
    unsigned short column = 0;
    try {
      while (ss >> number) {
        totalTime += number;
        // 4th column is idle time
        if (column == 3) {
          idleTime = number;
        }
        column++;
      }

      if (!utilSensors.contains(cpuCoreNum)) {
        spdlog::critical("somehow, cpu core is not present in the cpuUtil map. aborting");
        throw std::runtime_error("");
      }
      auto &utilEntry = utilSensors.at(cpuCoreNum);
      if (utilEntry.utilOld.hasRead) {
        const long long calc_totalTime = totalTime - utilEntry.utilOld.totalTime;
        const long long calc_idleTime = idleTime - utilEntry.utilOld.idleTime;
        utilEntry.utilOld.totalTime = totalTime;
        utilEntry.utilOld.idleTime = idleTime;

        // auto &coreStringStream = *utilSensorDescriptors.at(cpuCoreNum);
        auto &coreStringStream = utilEntry.dataStream;
        coreStringStream->str("");
        coreStringStream->clear();
        *coreStringStream << 100 * (calc_totalTime - calc_idleTime) / (double)calc_totalTime;
      } else {
        utilEntry.utilOld.totalTime = totalTime;
        utilEntry.utilOld.idleTime = idleTime;
        utilEntry.utilOld.hasRead = true;
      }
    } catch (const std::out_of_range &) {
      spdlog::critical("somehow, amount of cores read is invalid. aborting");
      return;
    } catch (const std::exception &) {
      spdlog::critical("reading cpu utilization failed. aborting");
      return;
    }
  }
}

void CpuDevice::read() {
  // spdlog::info("cpuUtil size is: {}", utilSensorDescriptors.size());
  // throw std::runtime_error("test");
  readCpuUtilization();
  Device::read();
}

void CpuDevice::initialize() {
  getCpuCoreFrequency();
  initCpuUtilization();
}
