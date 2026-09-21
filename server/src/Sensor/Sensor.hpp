#pragma once
#include <concepts>
#include <filesystem>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "Sensor/Source.hpp"
#include "SensorReading.hpp"
#include "SensorType.hpp"
#include "SourceFile.hpp"
#include "SourcePush.hpp"
#include "Transform.hpp"
#include "TransformDelta.hpp"
#include "TransformScale.hpp"

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

class Sensor {
public:
  Sensor(std::unique_ptr<Source> source, std::unique_ptr<Transform> readingTransform,
         SensorConfig config);

  void updateValue();
  SensorReading getReadings();
  void resetReadings();
  nlohmann::json serialize();
  void setPrimary(bool v);

  std::string getName();
  void setName(std::string name);
  SensorType getType();
  static SensorType deduceSensorType(const std::string &sensorName);

  template <std::derived_from<Transform> TTransform>
  static void makeFileSensor(std::vector<std::unique_ptr<Sensor>> &sensors,
                             const std::filesystem::path &streamPath, SensorConfig config);
  template <std::derived_from<Transform> TTransform>
  static SourcePush *addPushSensor(std::vector<std::unique_ptr<Sensor>> &sensors,
                                   SensorConfig config);

private:
  void accumulate(double value);

  std::unique_ptr<Source> source;
  std::unique_ptr<Transform> readingTransform;
  SensorConfig config;
  SensorReading readings;
};

template <std::derived_from<Transform> TTransform>
void Sensor::makeFileSensor(std::vector<std::unique_ptr<Sensor>> &sensors,
                            const std::filesystem::path &streamPath, SensorConfig config) {
  auto src = std::make_unique<SourceFile>(streamPath);
  auto transform = std::make_unique<TTransform>(config.divider());
  sensors.emplace_back(std::make_unique<Sensor>(std::move(src), std::move(transform), config));
}

template <std::derived_from<Transform> TTransform>
SourcePush *Sensor::addPushSensor(std::vector<std::unique_ptr<Sensor>> &sensors,
                                  SensorConfig config) {
  auto src = std::make_unique<SourcePush>();
  SourcePush *borrowed = src.get();
  auto transform = std::make_unique<TTransform>(config.divider());
  sensors.emplace_back(std::make_unique<Sensor>(std::move(src), std::move(transform), config));
  return borrowed;
}
