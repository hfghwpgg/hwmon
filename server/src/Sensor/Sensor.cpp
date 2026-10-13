#include "Sensor.hpp"

#include <cmath>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <utility>

#include "SensorReading.hpp"
#include "SensorType.hpp"
#include "Source.hpp"
#include "Transform.hpp"

Sensor::Sensor(std::unique_ptr<Source> source, std::unique_ptr<Transform> readingTransform,
               SensorConfig config, std::string_view ownerName) :
    ownerName(ownerName),
    source(std::move(source)),
    readingTransform(std::move(readingTransform)),
    config(config) {
  SPDLOG_TRACE("sensor init; its name: {}", config.name);
  readings.reset();
}

std::string Sensor::getName() {
  return config.name;
}
void Sensor::setName(std::string_view name) {
  config.name = name;
}
SensorType Sensor::getType() {
  return config.type;
}
void Sensor::setPrimary(bool v) {
  config.isPrimary = v;
}


void Sensor::updateValue() {
  // we count errors to 3, cuz sometimes bad reads happen
  // ex: on my machine, rarely gigabyte_wmi fails reading.
  // by rarely i mean once per 30 minutes (1s interval)
  if (readings.errorCount > MAX_ERRORS) {
    return;
  }
  if (readings.errorCount == MAX_ERRORS) {
    SPDLOG_ERROR("device: '{}' => sensor '{}': repeated invalid value read; disabling this sensor",
                 ownerName, config.name);
    readings.errorCount = 4;
    return;
  }

  auto raw = source->read();
  if (!raw) {
    if (raw.error() == Source::SourceStatus::NotReady)
      SPDLOG_TRACE("device: '{}' => sensor '{}': no value pushed yet", ownerName, config.name);
    else
      SPDLOG_TRACE("device: '{}' => sensor '{}': invalid value read ({})", ownerName, config.name,
                   readings.errorCount);

    readings.errorCount++;
    return;
  }

  auto val = readingTransform->apply(*raw);
  if (!val.has_value()) {
    SPDLOG_TRACE("device: '{}' => sensor '{}': not ready yet", ownerName, config.name);
    readings.errorCount++;
    return;
  }
  if (std::isnan(*val)) {
    SPDLOG_TRACE("device: '{}' => sensor '{}': value of recieved data is NaN", ownerName,
                 config.name);
    readings.errorCount++;
    return;
  }

  accumulate(*val);
}

void Sensor::accumulate(double value) {
  readings.value = value;
  readings.errorCount = 0;

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
