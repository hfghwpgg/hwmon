#pragma once
#include <concepts>
#include <filesystem>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "Sensor/Source.hpp"
#include "SensorReading.hpp"
#include "SensorType.hpp"
#include "Transform.hpp"

struct SensorConfig {
  std::string name;
  SensorType type;
  unsigned int rawDivider = 0; // 0 = getDivider(type)
  bool aggregateData = true;
  bool isPrimary = false;

  unsigned int divider() const {
    return (rawDivider == 0) ? getDivider(type) : rawDivider;
  }
};

// to add a sensor, use one makeFileSensor or addPushSensor
// for template, use derivative of Transform interface
class Sensor {
public:
  void updateValue();
  SensorReading getReadings();
  void resetReadings();
  nlohmann::json serialize();
  void setPrimary(bool v);

  std::string getName();
  void setName(std::string_view name);
  SensorType getType();
  static SensorType deduceSensorType(const std::string &sensorName);

  template <std::derived_from<Transform> TTransform>
  static void makeFileSensor(std::vector<std::unique_ptr<Sensor>> &sensors,
                             const std::filesystem::path &streamPath, SensorConfig config,
                             std::string_view ownerName);
  template <std::derived_from<Transform> TTransform>
  static PushSource *addPushSensor(std::vector<std::unique_ptr<Sensor>> &sensors,
                                   SensorConfig config, std::string_view ownerName);

private:
  Sensor(std::unique_ptr<Source> source, std::unique_ptr<Transform> readingTransform,
         SensorConfig config, std::string_view ownerName);

  void accumulate(double value);

  std::string_view ownerName;
  std::unique_ptr<Source> source;
  std::unique_ptr<Transform> readingTransform;
  SensorConfig config;
  SensorReading readings;
};

template <std::derived_from<Transform> TTransform>
void Sensor::makeFileSensor(std::vector<std::unique_ptr<Sensor>> &sensors,
                            const std::filesystem::path &streamPath, SensorConfig config,
                            std::string_view ownerName) {
  auto src = std::make_unique<FileSource>(streamPath);
  auto transform = std::unique_ptr<TTransform>(new TTransform(config.divider()));
  sensors.emplace_back(
      std::unique_ptr<Sensor>(new Sensor(std::move(src), std::move(transform), config, ownerName)));
}

template <std::derived_from<Transform> TTransform>
PushSource *Sensor::addPushSensor(std::vector<std::unique_ptr<Sensor>> &sensors,
                                  SensorConfig config, std::string_view ownerName) {
  auto src = std::make_unique<PushSource>();
  PushSource *borrowed = src.get();
  auto transform = std::unique_ptr<TTransform>(new TTransform(config.divider()));
  sensors.emplace_back(
      std::unique_ptr<Sensor>(new Sensor(std::move(src), std::move(transform), config, ownerName)));
  return borrowed;
}
