#pragma once
#include "helpers.hpp"
#include <cstddef>
#include <filesystem>

struct Config {
  unsigned int debuglevel;
  helpers::fs::path sockPath;
  helpers::fs::path hwmonPath;
  unsigned int initialIntervalMs;
  int backlog;
  size_t maxClients;
  bool refreshSocket;
  bool dontDropRoot;
};
Config CliParser(int argc, char *argv[]);
