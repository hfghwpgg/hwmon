#include "CliParser.hpp"
#include <argparse/argparse.hpp>
#include <filesystem>
#include <stdexcept>

Config CliParser(int argc, char *argv[]) {
  argparse::ArgumentParser hwmon("hwmon");
  hwmon.add_argument("-d", "--debug-level")
      .default_value(0u)
      .help("set debugging level\n0 - no debug logs\n1 - debug logs\n2 - trace logs (verbose)")
      .scan<'u', unsigned int>()
      .nargs(1)
      .action([](const std::string &val) {
        unsigned int parsed = std::stoul(val);
        if (parsed > 2) {
          throw std::runtime_error("Argument -d/--debuglevel must be less or equal to 2");
        }
        return static_cast<unsigned int>(parsed);
      });
  hwmon.add_argument("-i", "--interval")
      .default_value(1000u)
      .help("logging interval in ms")
      .scan<'u', unsigned int>()
      .nargs(1)
      .action([](const std::string &val) {
        unsigned long parsed = std::stoul(val);
        constexpr unsigned long min_interval = 50;

        if (parsed < min_interval) {
          throw std::runtime_error("Argument -i/--interval must be at least " +
                                   std::to_string(min_interval) + " ms");
        }
        return static_cast<unsigned int>(parsed);
      });
  hwmon.add_argument("--sock-path")
      .default_value(std::filesystem::path("/tmp/hwmon/hwmon.sock"))
      .help("file path for socket")
      .nargs(1)
      .action([](const std::string &val) {
        const std::filesystem::path retPath = std::filesystem::path(val);
        return static_cast<std::filesystem::path>(retPath);
      });
  hwmon.add_argument("--backlog")
      .default_value(3u)
      .help("amount of clients that can connect at once")
      .scan<'u', unsigned int>()
      .nargs(1);
  hwmon.add_argument("--hwmon-path")
      .default_value(std::filesystem::path("/sys/class/hwmon"))
      .help("path to sysfs hwmon (for testing)")
      .nargs(1)
      .action([](const std::string &val) {
        const std::filesystem::path retPath = std::filesystem::path(val);
        return static_cast<std::filesystem::path>(retPath);
      });
  hwmon.add_argument("--refresh-socket")
      .default_value(false)
      .help("force remove socket file and its folder")
      .flag();
  hwmon.add_argument("--dont-drop-root")
      .default_value(false)
      .help("dont drop root privileges (not recommended)\nmaybe useful if you're elevating with "
            "something different than sudo")
      .flag();
  try {
    hwmon.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << '\n';
    std::cerr << hwmon << '\n';
    throw std::runtime_error("failed to parse arguments");
  }

  Config config;
  config.debuglevel = hwmon.get<unsigned int>("--debug-level");
  config.sockPath = hwmon.get<std::filesystem::path>("--sock-path");
  config.hwmonPath = hwmon.get<std::filesystem::path>("--hwmon-path");
  config.initialIntervalMs = hwmon.get<unsigned int>("--interval");
  config.backlog = hwmon.get<unsigned int>("--backlog");
  config.refreshSocket = hwmon.get<bool>("--refresh-socket");
  config.dontDropRoot = hwmon.get<bool>("--dont-drop-root");

  return config;
}
