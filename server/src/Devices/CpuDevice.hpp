#pragma once
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>

#include "../Device.hpp"
#include "../PreadFile.hpp"
#include "../helpers.hpp"


class CpuDevice : public Device {
public:
  CpuDevice(std::set<std::filesystem::path> &hwmonPaths);
  CpuDevice(std::set<std::filesystem::path> &hwmonPaths, std::filesystem::path cpufreq_path,
            std::filesystem::path cpuinfo_path, std::filesystem::path cpuutil_path,
            std::filesystem::path intelrapl_path);

  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  struct lastReading { // cpu time
    unsigned long long totalTime;
    unsigned long long idleTime;
    bool hasRead;
  };

  struct utilSensorData { // cpu time
    SourcePush *src;
    lastReading utilOld;
  };

  const struct CpuPaths {
    std::filesystem::path cpufreq;
    std::filesystem::path cpuinfo;
    std::filesystem::path cpuutil;
    std::filesystem::path intelrapl;
  } cpuPaths;

  std::unordered_map<std::string, utilSensorData> utilSensorsPrivate;

  std::set<std::filesystem::path> &hwmonPaths;
  PreadFile cpuutilFile;

  void getTemperature();
  void getCoreFrequency();
  std::string getName();

  void initUtilization();
  void readUtilization();

  void getPowerDraw();

  // TODO:
  //       power
  //       vcore
};
