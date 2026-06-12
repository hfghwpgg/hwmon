#pragma once
#include <fmt/args.h>
#include <fmt/color.h>
#include <fmt/base.h>
#include <stddef.h>
#include <string_view>
#include <utility>

enum class LogLevel { INFO, WARNING, ERROR, CRITICAL };

class Logger {
private:
  static void printLog(LogLevel level, std::string_view tag, std::string_view message);
  static void debugLog(LogLevel level, std::string_view message, size_t skip);
  static fmt::terminal_color getTagColor(LogLevel level);

public:
  Logger() = delete;
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger &&) = delete;

  template <typename... Args> static void println(fmt::format_string<Args...> fmt, Args &&...args) {
    fmt::println("{}", fmt::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args> static void print(fmt::format_string<Args...> fmt, Args &&...args) {
    fmt::print("{}", fmt::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args> static void logInfo(fmt::format_string<Args...> fmt, Args &&...args) {
    printLog(LogLevel::INFO, "info", fmt::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void logWarning(fmt::format_string<Args...> fmt, Args &&...args) {
    printLog(LogLevel::WARNING, "warning", fmt::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void LogError(fmt::format_string<Args...> fmt, Args &&...args) {
    printLog(LogLevel::ERROR, "error", fmt::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void logCritical(fmt::format_string<Args...> fmt, Args &&...args) {
    printLog(LogLevel::CRITICAL, "critical", fmt::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void debugInfo(fmt::format_string<Args...> fmt, Args &&...args) {
#ifndef NDEBUG
    debugLog(LogLevel::INFO, fmt::format(fmt, std::forward<Args>(args)...), 1);
#endif
  }

  template <typename... Args>
  static void debugWarning(fmt::format_string<Args...> fmt, Args &&...args) {
#ifndef NDEBUG
    debugLog(LogLevel::WARNING, fmt::format(fmt, std::forward<Args>(args)...), 1);
#endif
  }
  template <typename... Args>
  static void debugError(fmt::format_string<Args...> fmt, Args &&...args) {
#ifndef NDEBUG
    debugLog(LogLevel::ERROR, fmt::format(fmt, std::forward<Args>(args)...), 1);
#endif
  }

  template <typename... Args>
  static void debugCritical(fmt::format_string<Args...> fmt, Args &&...args) {
#ifndef NDEBUG
    debugLog(LogLevel::CRITICAL, fmt::format(fmt, std::forward<Args>(args)...), 1);
#endif
  }
};
