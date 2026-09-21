#include "Sensor.hpp"

#include <cmath>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

#include "Sensor/SourceFile.hpp"
#include "Sensor/TransformScale.hpp"
#include "SensorReading.hpp"
#include "SensorType.hpp"
#include "Source.hpp"
#include "Transform.hpp"

Sensor::Sensor(std::unique_ptr<Source> source, std::unique_ptr<Transform> readingTransform,
               SensorConfig config) :
    source(std::move(source)),
    readingTransform(std::move(readingTransform)),
    config(config) {
  spdlog::trace("sensor init; its name: {}", config.name);
  readings.reset();
}

std::string Sensor::getName() {
  return config.name;
}
void Sensor::setName(std::string name) {
  config.name = name;
}
SensorType Sensor::getType() {
  return config.type;
}
void Sensor::setPrimary(bool v) {
  config.isPrimary = v;
}


void Sensor::updateValue() {
  auto raw = source->read();
  if (!raw) {
    if (raw.error() == Source::SourceStatus::NotReady)
      spdlog::trace("sensor '{}': no value pushed yet", config.name);
    else
      spdlog::error("sensor '{}': invalid value read", config.name);
    return;
  }

  auto val = readingTransform->apply(*raw);
  if (!val.has_value()) {
    spdlog::trace("sensor '{}': not ready yet", config.name);
    return;
  }
  if (std::isnan(*val)) {
    spdlog::trace("sensor '{}': value of recieved data is NaN", config.name);
    return;
  }

  accumulate(*val);
}

void Sensor::accumulate(double value) {
  readings.value = value;

  if (!config.aggregateData) {
    readings.sum = value;
    readings.times = 1;
  } else {
    readings.sum += value;
    readings.times++;
  }

  if (readings.min_value > value || std::isnan(readings.min_value)) {
    readings.min_value = value;
  }
  if (readings.max_value < value || std::isnan(readings.max_value)) {
    readings.max_value = value;
  }
}

SensorReading Sensor::getReadings() {
  return readings;
}

void Sensor::resetReadings() {
  source->reset();
  readingTransform->reset();
  readings.reset();
}

nlohmann::json Sensor::serialize() {
  nlohmann::json j;
  j["name"] = config.name;
  j["type"] = config.type;
  j["isPrimary"] = config.isPrimary;
  j["readings"] = readings.serialize();
  return j;
}

SensorType Sensor::deduceSensorType(const std::string &sensorName) {
  auto lastDigit = sensorName.find_first_of("0123456789");

  std::string_view prefix;
  if (lastDigit != std::string::npos) {
    prefix = std::string_view(sensorName).substr(0, lastDigit);
  } else {
    prefix = sensorName;
  }
  auto it = sensorConfigMap.find(prefix);
  if (it != sensorConfigMap.end()) {
    return it->second;
  }
  // fallback
  return SensorType::UNKNOWN;
}
