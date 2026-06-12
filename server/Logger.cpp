#include "Logger.hpp"
#include <fmt/color.h>
#include <stacktrace>
#include <string>
#include <unordered_set>

using enum LogLevel;

static std::unordered_set<std::string> filtered_files = {};

fmt::terminal_color Logger::getTagColor(LogLevel level) {
  switch (level) {
  case INFO:
    return fmt::terminal_color::green;
  case WARNING:
    return fmt::terminal_color::yellow;
  case ERROR:
  case CRITICAL:
    return fmt::terminal_color::red;
  default:
    return fmt::terminal_color::white;
  }
}

void Logger::printLog(LogLevel level, std::string_view tag, std::string_view message) {
  auto tag_color = getTagColor(level);

  fmt::print(fmt::emphasis::bold | fg(tag_color), "[{}] ", tag);
  if (level == CRITICAL) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", message);
  } else {
    fmt::print("{}\n", message);
  }
}

void Logger::debugLog(LogLevel level, std::string_view message, size_t skip) {
  auto stack_current = std::stacktrace::current();
  std::string filename = "?";
  long line_number = 0;

  skip += 1;

  if (skip < stack_current.size()) {
    auto source_file = stack_current[skip].source_file();
    auto last_slash = source_file.find_last_of('/');
    filename = source_file.substr(last_slash == std::string::npos ? 0 : last_slash + 1);
    line_number = stack_current[skip].source_line();
  }

  if (filtered_files.contains(filename)) {
    return;
  }

  auto tag_color = getTagColor(level);

  fmt::print(fmt::emphasis::bold | fg(tag_color), "[{}:{}] ", filename, line_number);
  if (level == CRITICAL) {
    fmt::print(fg(fmt::terminal_color::bright_red), "{}\n", message);
  } else {
    fmt::print("{}\n", message);
  }
}
