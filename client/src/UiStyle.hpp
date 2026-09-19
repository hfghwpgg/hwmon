#pragma once

#include <QColor>
#include <QFont>

inline constexpr double kHotTemperatureC = 90.0;

inline QColor warningColor(bool dark) {
  return dark ? QColor("#e06c75") : QColor("#c4474a");
}


// Point sizes for labels and readings. 0 keeps the Qt/application default.
inline constexpr int kFontDeviceName = 11;
inline constexpr int kFontSectionName = 11;
inline constexpr int kFontSensorName = 11;
inline constexpr int kFontSensorReading = 11;
inline constexpr int kFontElapsed = 12; // 0 => default size + 2
inline constexpr int kFontStatus = 12;

inline QFont sizedFont(QFont font, int pointSize) {
  if (pointSize > 0) {
    font.setPointSize(pointSize);
  }
  return font;
}
