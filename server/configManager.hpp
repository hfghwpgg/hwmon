#pragma once
#include <filesystem>

struct Config {
  unsigned int debuglevel;
  std::filesystem::path sockPath;
  std::filesystem::path hwmonPath;
  unsigned int initialIntervalMs;
  int backlog;
  bool refreshSocket;
};
Config configManager(int argc, char *argv[]);
