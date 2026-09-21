#pragma once

#include <QColor>
#include <QFont>

inline constexpr double kHotTemperatureC = 90.0;

// Every colour used by the client, keyed by role. Palette, stylesheet and the
// model's item colours all read from here so a theme lives in a single place.
struct ThemeColors {
  QColor accent;
  QColor text;
  QColor mutedText;
  QColor disabledText;
  QColor warning;

  QColor window;     // QPalette::Window, QMenu background
  QColor mainWindow; // QMainWindow background
  QColor base;       // QPalette::Base
  QColor treeBase;
  QColor alternateBase;
  QColor dialog;
  QColor header;
  QColor control; // QPushButton / QSpinBox background (stylesheet)
  QColor button;  // QPalette::Button
  QColor buttonHover;
  QColor border;
  QColor light;
  QColor midlight;
  QColor mid;
  QColor dark;
  QColor shadow;
  QColor highlightedText;
  QColor tooltipBase;
  QColor selection;
  QColor menuSelection;
  QColor radioBorder;
  QColor radioBase;
  QColor footer;
  QColor hintBackground;
  QColor hintText;

  QColor deviceRow;
  QColor sectionRow;
  QColor flash;
};

inline const ThemeColors &themeColors(bool dark) {
  static const ThemeColors darkTheme{
      .accent = QColor("#ececec"),
      .text = QColor("#ececec"),
      .mutedText = QColor("#9aa3ad"),
      .disabledText = QColor("#66707a"),
      .warning = QColor("#e06c75"),
      .window = QColor("#1e1e1e"),
      .mainWindow = QColor("#141414"),
      .base = QColor("#141414"),
      .treeBase = QColor("#1b1b1b"),
      .alternateBase = QColor("#222222"),
      .dialog = QColor("#2a2a2a"),
      .header = QColor("#2a2a2a"),
      .control = QColor("#333333"),
      .button = QColor("#333333"),
      .buttonHover = QColor("#404040"),
      .border = QColor("#3a3a3a"),
      .light = QColor("#2a2a2a"),
      .midlight = QColor("#2a2a2a"),
      .mid = QColor("#3a3a3a"),
      .dark = QColor("#3a3a3a"),
      .shadow = QColor("#000000"),
      .highlightedText = QColor("#141414"),
      .tooltipBase = QColor("#2a2a2a"),
      .selection = QColor("#2d3d4e"),
      .menuSelection = QColor("#2d3d4e"),
      .radioBorder = QColor("#8a8a8a"),
      .radioBase = QColor("#1a1a1a"),
      .footer = QColor("#202020"),
      .hintBackground = QColor("#3a3a3a"),
      .hintText = QColor("#d0d0d0"),
      .deviceRow = QColor("#00000000"),
      .sectionRow = QColor("#00000000"),
      .flash = QColor(108, 182, 255, 26),
  };
  static const ThemeColors lightTheme{
      .accent = QColor("#1a1a1a"),
      .text = QColor("#1a1a1a"),
      .mutedText = QColor("#66707a"),
      .disabledText = QColor("#9a9a9a"),
      .warning = QColor("#c4474a"),
      .window = QColor("#ffffff"),
      .mainWindow = QColor("#f4f4f4"),
      .base = QColor("#ffffff"),
      .treeBase = QColor("#fafafa"),
      .alternateBase = QColor("#f0f0f0"),
      .dialog = QColor("#ffffff"),
      .header = QColor("#e6e6e6"),
      .control = QColor("#ffffff"),
      .button = QColor("#f4f4f4"),
      .buttonHover = QColor("#f0f0f0"),
      .border = QColor("#cfcfcf"),
      .light = QColor("#ffffff"),
      .midlight = QColor("#e6e6e6"),
      .mid = QColor("#cfcfcf"),
      .dark = QColor("#9a9a9a"),
      .shadow = QColor("#b0b0b0"),
      .highlightedText = QColor("#ffffff"),
      .tooltipBase = QColor("#ffffff"),
      .selection = QColor("#c9dbeb"),
      .menuSelection = QColor("#d7e4f2"),
      .radioBorder = QColor("#6a6a6a"),
      .radioBase = QColor("#ffffff"),
      .footer = QColor("#ececec"),
      .hintBackground = QColor("#f3f3f3"),
      .hintText = QColor("#555555"),
      .deviceRow = QColor("#00000000"),
      .sectionRow = QColor("#00000000"),
      .flash = QColor(21, 101, 192, 26),
  };
  return dark ? darkTheme : lightTheme;
}

inline QColor accentColor(bool dark) {
  return themeColors(dark).accent;
}

inline QColor warningColor(bool dark) {
  return themeColors(dark).warning;
}

// Point sizes for labels and readings. 0 keeps the Qt/application default.
inline constexpr int kFontDeviceName = 10;
inline constexpr int kFontSectionName = 10;
inline constexpr int kFontSensorName = 10;
inline constexpr int kFontSensorReading = 10;
inline constexpr int kFontElapsed = 12; // 0 => default size + 2
inline constexpr int kFontStatus = 12;

inline QFont sizedFont(QFont font, int pointSize) {
  if (pointSize > 0) {
    font.setPointSize(pointSize);
  }
  return font;
}
