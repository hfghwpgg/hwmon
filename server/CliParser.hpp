#pragma once
#include "hwmon.hpp"
#include <filesystem>

struct Config {
  unsigned int debuglevel;
  hwmon::fs::path sockPath;
  hwmon::fs::path hwmonPath;
  unsigned int initialIntervalMs;
  int backlog;
  bool refreshSocket;
  bool dontDropRoot;
};
Config CliParser(int argc, char *argv[]);
