#pragma once
struct SensorReading {
  float value;
  float min_value;
  float max_value;
  double sum;
  unsigned int times;
};