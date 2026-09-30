#pragma once
#include "helpers.hpp"
#include <cstddef>
#include <filesystem>

struct Config {
  unsigned int debuglevel;
  std::filesystem::path hwmonPath;
  unsigned int initialIntervalMs;
  int backlog;
  size_t maxClients;
  bool dontDropRoot;
};
Config CliParser(int argc, char *argv[]);
