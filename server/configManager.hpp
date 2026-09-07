#pragma once
#include <string>

struct Config {
  bool debug;
  std::string sockFolder;
  std::string sockPath;
  std::string hwmonPath;
  unsigned int initialIntervalMs;
  int backlog;
};
Config configManager(int argc, char *argv[]);
