#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Sensor.hpp"
#include "SensorType.hpp"

// Sensor whose value is pushed in by its owner instead of being read from a
// file. Used by devices backed by vendor libraries (NVML, ROCm SMI, i915 PMU).
class ValueSensor : public Sensor {
public:
  ValueSensor(std::string name, SensorType type, bool aggregateData = true);

  // value is expected in the unit the SensorType is serialized in
  // (C, MHz, W, %, bytes, bytes/s); no divider is applied
  void setValue(long double value);
  void invalidate();

  void resetReadings() override;

private:
  long double prepareValue() override;

  long double pendingValue;
};

// appends a ValueSensor to a device's sensor list and hands back a borrowed
// pointer the device uses to push values into it
ValueSensor *addValueSensor(std::vector<std::unique_ptr<Sensor>> &sensors, std::string name,
                            SensorType type, bool aggregateData = true);
