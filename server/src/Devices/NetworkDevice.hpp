#pragma once
#include "Device.hpp"
#include "Sensor/Sensor.hpp"
#include "Sensor/SourcePush.hpp"

#include <array>
#include <fstream>
#include <map>
#include <string>
#include <vector>


class NetworkDevice : public Device {
public:
  NetworkDevice(std::string name, std::filesystem::path netDev);
  NetworkDevice(std::string name);
  ~NetworkDevice();

  std::map<std::string, std::array<unsigned long long, 2>> parseData();
  void initialize() override;
  void read() override;
  void resetReadings() override;
  nlohmann::json serialize() override;

private:
  std::vector<SourcePush *> Sources;
  std::ifstream netDevFD;
};
