#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "../Device.hpp"
#include "../Sensor.hpp"
#include "../SensorType.hpp"
#include "CpuDevice.hpp"
#include "SharedHwmonParser.hpp"
#include "helpers.hpp"

namespace fs = std::filesystem;

CpuDevice::CpuDevice(std::set<fs::path> &hwmonPaths) :
    CpuDevice(hwmonPaths, "/sys/devices/system/cpu/cpufreq/", "/proc/cpuinfo", "/proc/stat") {}

CpuDevice::CpuDevice(std::set<std::filesystem::path> &hwmonPaths, fs::path CPUFREQ_PATH,
                     fs::path CPUINFO_PATH, fs::path CPUUTIL_PATH) :
    Device("cpu", DeviceType::CPU),
    CPUFREQ_PATH(CPUFREQ_PATH),
    CPUINFO_PATH(CPUINFO_PATH),
    CPUUTIL_PATH(CPUUTIL_PATH),
    hwmonPaths(hwmonPaths) {}

CpuDevice::~CpuDevice() {
  if (CPUUTIL_FD.is_open())
    CPUUTIL_FD.close();
}

void CpuDevice::initialize() {
  name = getName();
  getTemperature();
  getCoreFrequency();
  initUtilization();
}

void CpuDevice::read() {
  readUtilization();
  Device::read();
}

void CpuDevice::resetReadings() {
  for (auto &entry : utilSensors) {
    entry.second.utilOld.hasRead = false;
    entry.second.dataStream->str("");
    entry.second.dataStream->clear();
  }
  Device::resetReadings();
}

void CpuDevice::getTemperature() {
  fs::path coretempDriver = ""; // placeholders
  fs::path cpuTemp = "";

  for (const fs::path &dir : hwmonPaths) {
    if (dir.string().contains("nvme"))
      continue;

    if (dir.string().contains("coretemp")) {
      coretempDriver = dir;
      break;
    }

    for (const auto &file : fs::directory_iterator(dir)) {
      const auto filename = file.path().stem().string();
      if (!filename.contains("label"))
        continue;

      const auto label = helpers::readFileFirstLine(file.path());
      if (label.starts_with("Package id") || label.starts_with("Tdie") ||
          label.starts_with("SoC Temperature")) {
        cpuTemp = dir;
      } else if (label.starts_with("Core") || label.starts_with("Tccd")) {
        coretempDriver = dir;
      }
    }
  }

  for (const auto &dir : {cpuTemp, coretempDriver}) {
    if (helpers::pathType(dir) == helpers::pathTypeEnum::INVALID)
      continue;

    const auto available_sensors = SharedHwmonParser::parseHwmonDirectory(dir);
    SharedHwmonParser::createSensors(dir, available_sensors, sensors);
    hwmonPaths.erase(dir);
  }
}

// this interface returns frequency in kHz, not Hz.
void CpuDevice::getCoreFrequency() {
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

std::string CpuDevice::getName() {
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
// stringstreams for cpu + each core
void CpuDevice::initUtilization() {
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
    ss >> cpuCoreNum; // first column is name
    utilSensors.emplace(cpuCoreNum,
                        utilSensorData{std::make_shared<std::stringstream>(), {0, 0, false}});
    sensors.emplace_back(std::make_unique<Sensor>(utilSensors.at(cpuCoreNum).dataStream, cpuCoreNum,
                                                  SensorType::UTILIZATION));
  }
}

// actually reading stuff
void CpuDevice::readUtilization() {
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
    // this shouldnt happen, but wont hurt i guess
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
        throw std::runtime_error("cpuCoreNum not present in cpuUtil map");
      }
      auto &utilEntry = utilSensors.at(cpuCoreNum);
      if (utilEntry.utilOld.hasRead) {
        // calculations
        const long long calc_totalTime = totalTime - utilEntry.utilOld.totalTime;
        const long long calc_idleTime = idleTime - utilEntry.utilOld.idleTime;
        utilEntry.utilOld.totalTime = totalTime;
        utilEntry.utilOld.idleTime = idleTime;

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
      throw std::runtime_error("out of range in CpuDevice");
    } catch (const std::exception &) {
      spdlog::critical("reading cpu utilization failed. aborting");
      throw std::runtime_error("reading cpu utilization failed");
    }
  }
}
