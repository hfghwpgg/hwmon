#include "Sensor.hpp"

#include <cmath>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

#include "SensorReading.hpp"
#include "SensorType.hpp"
#include "Source.hpp"
#include "Transform.hpp"

Sensor::Sensor(std::unique_ptr<Source> source, std::unique_ptr<Transform> readingTransform,
               SensorConfig config) :
    source(std::move(source)),
    readingTransform(std::move(readingTransform)),
    config(config) {
  SPDLOG_TRACE("sensor init; its name: {}", config.name);
  readings.reset();
}

std::string Sensor::getName() {
  return config.name;
}
void Sensor::setName(std::string name) {
  config.name = std::move(name);
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
      SPDLOG_TRACE("sensor '{}': no value pushed yet", config.name);
    else
      SPDLOG_ERROR("sensor '{}': invalid value read", config.name);
    return;
  }

  auto val = readingTransform->apply(*raw);
  if (!val.has_value()) {
    SPDLOG_TRACE("sensor '{}': not ready yet", config.name);
    return;
  }
  if (std::isnan(*val)) {
    SPDLOG_TRACE("sensor '{}': value of recieved data is NaN", config.name);
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

// disambiguate unfortunate name for those
// (unique_ptr `reset` vs my type `reset`)
void Sensor::resetReadings() {
  source.get()->reset();
  readingTransform.get()->reset();
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
  auto firstDigit = sensorName.find_first_of("0123456789");

  std::string_view prefix;
  if (firstDigit != std::string::npos) {
    prefix = std::string_view(sensorName).substr(0, firstDigit);
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
