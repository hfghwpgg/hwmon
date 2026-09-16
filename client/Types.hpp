#pragma once

#include <QString>

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
  default:
    return QStringLiteral("DEV");
  }
}

inline QString sensorTypeLabel(int type) {
  switch (type) {
  case 0:
    return QStringLiteral("TEMP");
  case 1:
    return QStringLiteral("FAN");
  case 2:
    return QStringLiteral("CLK");
  case 3:
    return QStringLiteral("PWR");
  case 4:
    return QStringLiteral("VOLT");
  case 5:
    return QStringLiteral("CURR");
  case 6:
    return QStringLiteral("ENRG");
  case 7:
    return QStringLiteral("UTIL");
  case 8:
    return QStringLiteral("MEM");
  case 9:
    return QStringLiteral("THRU");
  default:
    return QStringLiteral("UNK");
  }
}

inline QString sensorIconPath(int type) {
  switch (type) {
  case 0:
    return QStringLiteral(":/icons/temperature.svg");
  case 1:
    return QStringLiteral(":/icons/fan.svg");
  case 2:
  case 7:
  case 8:
  case 9:
    return QStringLiteral(":/icons/clock.svg");
  case 3:
  case 4:
  case 5:
  case 6:
    return QStringLiteral(":/icons/voltage.svg");
  default:
    return {};
  }
}

inline QString deviceIconPath(int type) {
  if (type == 4) {
    return QStringLiteral(":/icons/chip.svg");
  }
  return {};
}

inline QString sensorSectionName(int type) {
  switch (type) {
  case 0:
    return QStringLiteral("Temperatures");
  case 1:
    return QStringLiteral("Fans");
  case 2:
    return QStringLiteral("Clocks");
  case 3:
    return QStringLiteral("Power");
  case 4:
    return QStringLiteral("Voltages");
  case 5:
    return QStringLiteral("Currents");
  case 6:
    return QStringLiteral("Energy");
  case 7:
    return QStringLiteral("Utilization");
  case 8:
    return QStringLiteral("Memory");
  case 9:
    return QStringLiteral("Throughput");
  default:
    return QStringLiteral("Other");
  }
}
