#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <set>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "../Device.hpp"
#include "../Sensor/Sensor.hpp"
#include "../Sensor/SensorType.hpp"
#include "CpuDevice.hpp"
#include "DeviceType.hpp"
#include "Sensor/TransformDelta.hpp"
#include "Sensor/TransformScale.hpp"
#include "SharedHwmonParser.hpp"
#include "helpers.hpp"

CpuDevice::CpuDevice(std::set<std::filesystem::path> &hwmonPaths) :
    CpuDevice(hwmonPaths, "/sys/devices/system/cpu/cpufreq/", "/proc/cpuinfo", "/proc/stat",
              "/sys/class/powercap/intel-rapl:0/energy_uj") {}

CpuDevice::CpuDevice(std::set<std::filesystem::path> &hwmonPaths,
                     std::filesystem::path cpufreq_path, std::filesystem::path cpuinfo_path,
                     std::filesystem::path cpuutil_path, std::filesystem::path intelrapl_path) :
    Device("SAMPLE CPU NAME", DeviceType::CPU),
    cpuPaths(CpuPaths{.cpufreq = cpufreq_path,
                      .cpuinfo = cpuinfo_path,
                      .cpuutil = cpuutil_path,
                      .intelrapl = intelrapl_path}),
    hwmonPaths(hwmonPaths) {}

CpuDevice::~CpuDevice() {
  if (cpuutil_fd.is_open())
    cpuutil_fd.close();
}

void CpuDevice::initialize() {
  name = getName();
  getTemperature();
  getCoreFrequency();
  initUtilization();
  getPowerDraw();
}

void CpuDevice::read() {
  readUtilization();
  for (const auto &sensor : sensors) {
    sensor->updateValue();
  }
}

void CpuDevice::resetReadings() {
  // for (auto &entry : utilSensorsPrivate) {
  //   auto &e = entry.second;
  //   e.utilOld.hasRead = false;
  //   e.utilOld.totalTime = 0;
  //   e.utilOld.idleTime = 0;
  // }
  for (const auto &sensor : sensors) {
    sensor->resetReadings();
  }
}

nlohmann::json CpuDevice::serialize() {
  nlohmann::json j;
  j["name"] = name;
  j["type"] = type;
  for (const auto &sensor : sensors) {
    j["sensors"].push_back(sensor->serialize());
  }
  return j;
}

void CpuDevice::getTemperature() {
  // placeholders
  std::filesystem::path coretempDriver = "";
  std::filesystem::path cpuTemp = "";

  for (const std::filesystem::path &dir : hwmonPaths) {
    if (dir.string().contains("nvme"))
      continue;

    if (dir.string().contains("coretemp")) {
      coretempDriver = dir;
      break;
    }

    for (const auto &file : std::filesystem::directory_iterator(dir)) {
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
  if (!std::filesystem::exists(cpuPaths.cpufreq) || access(cpuPaths.cpufreq.c_str(), R_OK) == -1) {
    spdlog::error("{} inaccessible", cpuPaths.cpufreq.string());
    return;
  }
  for (const auto &policy : std::filesystem::directory_iterator(cpuPaths.cpufreq)) {
    const std::string filename = policy.path().stem().string();
    if (!filename.starts_with("policy"))
      continue;

    auto path = policy.path() / "scaling_cur_freq";
    // we know that file starts with 'policy', and thats 6 letters.
    // we only want core number, so we substr the beginning
    const std::string suffix = filename.substr(6);
    const std::string label = suffix.length() > 0 ? "CPU core " + suffix : "CPU";
    Sensor::makeFileSensor<TransformScale>(sensors, path, {label, SensorType::FREQUENCY, 1000});
  }
}

std::string CpuDevice::getName() {
  std::string name = "cpumodel"; // placeholder
  if (!std::filesystem::exists(cpuPaths.cpuinfo) || access(cpuPaths.cpuinfo.c_str(), R_OK) == -1) {
    spdlog::error("{} inaccessible; setting general name for cpu", cpuPaths.cpuinfo.string());
    return name;
  }

  std::ifstream cpuinfo_fd(cpuPaths.cpuinfo);
  std::string line;
  while (std::getline(cpuinfo_fd, line)) {
    if (line.find("model name") == std::string::npos)
      continue;

    const auto colonIdx = line.find(':');
    if (colonIdx == std::string::npos)
      continue;

    line = line.substr(colonIdx + 1);
    name = helpers::trim(line);
    break;
  }
  cpuinfo_fd.close();

  if (name == "cpumodel")
    spdlog::error("couldn't find cpu name in CPUINFO");
  return name;
}

// this is just creating right amount of
// valueSensors for cpu + each core
void CpuDevice::initUtilization() {
  if (!std::filesystem::exists(cpuPaths.cpuutil) || access(cpuPaths.cpuutil.c_str(), R_OK) == -1) {
    spdlog::error("{} inaccessible", cpuPaths.cpuutil.string());
    return;
  }

  cpuutil_fd.open(cpuPaths.cpuutil);
  std::string line;
  while (std::getline(cpuutil_fd, line)) {
    line = helpers::trim(line);
    if (!line.starts_with("cpu")) {
      continue;
    }
    std::stringstream ss(line);
    std::string cpuCoreNum;
    ss >> cpuCoreNum; // first column is name
    // remove cpu beginning
    const std::string suffix = cpuCoreNum.substr(3);
    const std::string label = suffix.length() > 0 ? "CPU core " + suffix : "CPU";
    const bool primary = label == "CPU";

    utilSensorsPrivate.emplace(
        cpuCoreNum, utilSensorData{Sensor::addPushSensor<TransformScale>(
                                       sensors, {label, SensorType::UTILIZATION, 1, true, primary}),
                                   {0, 0, false}});
  }
}

// actually reading stuff
void CpuDevice::readUtilization() {
  if (utilSensorsPrivate.size() == 0) {
    spdlog::critical("no cpu utilization sensors detected");
    return;
  }
  if (!cpuutil_fd.is_open()) {
    spdlog::critical("access to /proc/stat suddenly lost");
    return;
  }

  cpuutil_fd.clear();
  cpuutil_fd.seekg(0);

  std::string line;
  while (std::getline(cpuutil_fd, line)) {
    // this shouldnt happen, but wont hurt i guess
    if (!line.starts_with("cpu"))
      continue;

    std::stringstream ss(line);
    std::string cpuCoreNum;
    ss >> cpuCoreNum; // cpu or cpuN

    unsigned long number;        // placeholder for readings
    unsigned long totalTime = 0; // cpu time
    unsigned long idleTime = 0;  // cpu time
    unsigned short column = 0;
    try {
      while (ss >> number) {
        // guest and guest_nice (columns 9 and 10) are already included
        // in user and nice, so counting them would double the time
        if (column < 8) {
          totalTime += number;
        }
        // idle (4th column) and iowait (5th column); some kernels park
        // the whole idle time of a core in iowait
        if (column == 3 || column == 4) {
          idleTime += number;
        }
        column++;
      }

      if (!utilSensorsPrivate.contains(cpuCoreNum)) {
        spdlog::critical("somehow, cpu core is not present in the cpuUtil map. aborting");
        throw std::runtime_error("cpuCoreNum not present in cpuUtil map");
      }
      auto &utilEntry = utilSensorsPrivate.at(cpuCoreNum);
      if (utilEntry.utilOld.hasRead) {
        // calculations
        // time there corresponds to cpu time
        const long calc_totalTime = totalTime - utilEntry.utilOld.totalTime;
        const long calc_idleTime = idleTime - utilEntry.utilOld.idleTime;
        utilEntry.utilOld.totalTime = totalTime;
        utilEntry.utilOld.idleTime = idleTime;

        const double value =
            100 * (calc_totalTime - calc_idleTime) / static_cast<double>(calc_totalTime);
        utilEntry.src->setValue(value);
      } else {
        utilEntry.utilOld.totalTime = totalTime;
        utilEntry.utilOld.idleTime = idleTime;
        utilEntry.utilOld.hasRead = true;
        utilEntry.src->setValue(NAN); // follow convention
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

void CpuDevice::getPowerDraw() {
  // we use zenergy primarly
  // if its not present we try
  // to use intel rapl

  // intel rapl requires root to be read
  const bool intelRaplAccessible = (std::filesystem::exists(cpuPaths.intelrapl) &&
                                    access(cpuPaths.intelrapl.c_str(), R_OK) != -1);

  std::filesystem::path zenergyPath = "";
  for (const auto &dir : hwmonPaths) {
    if (dir.string().contains("zenergy")) {
      zenergyPath = dir;
      hwmonPaths.erase(dir);
      break;
    }
  }

  // there also apperently exists amd_energy
  // but i dont have it on my system
  const bool zenergyAccessible =
      (!zenergyPath.empty() && helpers::pathType(zenergyPath) == helpers::pathTypeEnum::DIRECTORY);

  helpers::SensorVec powerSensors;
  if (zenergyAccessible) {
    spdlog::info("using zenergy interface for cpu power draw");
    const auto availableSensors = SharedHwmonParser::parseHwmonDirectory(zenergyPath);
    powerSensors = SharedHwmonParser::returnSensors(zenergyPath, availableSensors);
    // kinda hacky
    for (const auto &sensor : powerSensors) {
      const auto name = sensor->getName();
      if (name.contains("socket")) {
        // Esocket has 7 letters
        const auto num = name.substr(7);
        const int number = std::stoi(num);
        sensor->setName(std::format("Socket {} power draw", number));
        sensor->setPrimary(true);
      }
      if (name.contains("core")) {
        // Ecore has 5 letters
        const auto num = name.substr(5);
        const int number = std::stoi(num);
        sensor->setName(std::format("Core {} power draw", number));
      }
    }
  } else if (intelRaplAccessible) {
    spdlog::info("using intel rapl interface for cpu power draw");
    Sensor::makeFileSensor<TransformDelta>(powerSensors, cpuPaths.intelrapl,
                                           {"Socket power draw", SensorType::POWER});
  } else {
    spdlog::error("couldn't read cpu power draw. Try running with sudo");
  }

  sensors.insert(sensors.end(), std::make_move_iterator(powerSensors.begin()),
                 std::make_move_iterator(powerSensors.end()));
}
