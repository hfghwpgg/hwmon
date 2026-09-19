#pragma once
#include "helpers.hpp"
#include <filesystem>

struct Config {
  unsigned int debuglevel;
  helpers::fs::path sockPath;
  helpers::fs::path hwmonPath;
  unsigned int initialIntervalMs;
  int backlog;
  bool refreshSocket;
  bool dontDropRoot;
};
Config CliParser(int argc, char *argv[]);
