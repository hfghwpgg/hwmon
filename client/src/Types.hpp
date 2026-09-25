#pragma once

#include <QString>
#include <QStringView>

// Mirrors the server's SensorType enum (0..9 known, 10 unknown).
inline constexpr int kUnknownSensorType = 10;
inline constexpr int kMemorySensorType = 8;
inline constexpr int kThroughputSensorType = 9;

struct SensorTypeInfo {
  QStringView label;
  QStringView sectionName;
  QStringView iconPath;
  QStringView unit;
  int precision;
};

inline constexpr SensorTypeInfo kSensorTypes[] = {
    {u"TEMP", u"Temperatures", u":/icons/temperature.svg", u" °C", 1},
    {u"FAN", u"Fans", u":/icons/fan.svg", u" RPM", 0},
    {u"CLK", u"Clocks", u":/icons/clock.svg", u" MHz", 1},
    {u"PWR", u"Power", u":/icons/voltage.svg", u" W", 1},
    {u"VOLT", u"Voltages", u":/icons/voltage.svg", u" V", 3},
    {u"CURR", u"Currents", u":/icons/voltage.svg", u" A", 3},
    {u"ENRG", u"Energy", u":/icons/voltage.svg", u" W", 3},
    {u"UTIL", u"Utilization", u":/icons/clock.svg", u" %", 1},
    {u"MEM", u"Memory", u":/icons/clock.svg", u"", 0},
    {u"THRU", u"Throughput", u":/icons/clock.svg", u"/s", 0},
};

inline constexpr SensorTypeInfo kUnknownSensorInfo{u"UNK", u"Other", u"", u"", 0};

inline constexpr const SensorTypeInfo &sensorTypeInfo(int type) {
  constexpr int count = static_cast<int>(sizeof(kSensorTypes) / sizeof(kSensorTypes[0]));
  return type >= 0 && type < count ? kSensorTypes[type] : kUnknownSensorInfo;
}

inline QString sensorTypeLabel(int type) {
  return sensorTypeInfo(type).label.toString();
}

inline QString sensorSectionName(int type) {
  return sensorTypeInfo(type).sectionName.toString();
}

inline QString sensorIconPath(int type) {
  return sensorTypeInfo(type).iconPath.toString();
}

inline QString deviceTypeLabel(int type) {
  switch (type) {
  case 0:
    return QStringLiteral("CPU");
  case 1:
    return QStringLiteral("GPU");
  case 2:
    return QStringLiteral("RAM");
  case 3:
    return QStringLiteral("STOR");
  case 4:
    return QStringLiteral("NET");
  default:
    return QStringLiteral("DEV");
  }
}

inline QString deviceIconPath(int type) {
  return type == 5 ? QStringLiteral(":/icons/chip.svg") : QString();
}
