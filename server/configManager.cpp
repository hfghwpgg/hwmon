#include "configManager.hpp"
#include <argparse/argparse.hpp>
#include <string>

#ifdef DEBUG
constexpr inline bool is_debug_build = true;
#else
constexpr inline bool is_debug_build = false;
#endif

Config configManager(int argc, char *argv[]) {
  argparse::ArgumentParser hwmon("hwmon");
  hwmon.add_argument("-d", "--debug").help("enable debug logging").flag().default_value(false);
  hwmon.add_argument("-i", "--interval")
      .default_value(1000u)
      .help("logging interval in ms")
      .scan<'u', unsigned int>()
      .action([](const std::string &val) {
        unsigned long parsed = std::stoul(val);
        constexpr unsigned long min_interval = 50;

        if (parsed < min_interval) {
          throw std::runtime_error("Argument -i/--interval must be at least " +
                                   std::to_string(min_interval) + " ms");
        }
        return static_cast<unsigned int>(parsed);
      })
      .nargs(1);
  hwmon.add_argument("--sockfolder")
      .help("folder for socket file")
      .default_value("/tmp/hwmon")
      .nargs(1);
  // NOTE: make sure we add slash between folder and file
  hwmon.add_argument("--sockfile").help("filename for socket").default_value("hwmon.sock").nargs(1);
  hwmon.add_argument("--backlog")
      .help("amount of clients that can connect at once")
      .default_value(3u)
      .scan<'u', unsigned int>()
      .nargs(1);
  hwmon.add_argument("--hwmonpath")
      .help("path to sysfs hwmon (for testing)")
      .default_value("/sys/class/hwmon")
      .nargs(1);

  try {
    hwmon.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << '\n';
    std::cerr << hwmon << '\n';
    throw std::runtime_error("failed to parse arguments");
  }

  Config config;
  config.debug = hwmon.get<bool>("--debug") || is_debug_build;
  config.sockFolder = hwmon.get<std::string>("--sockfolder");
  config.sockPath = config.sockFolder + "/" + hwmon.get<std::string>("--sockfile");
  config.hwmonPath = hwmon.get<std::string>("--hwmonpath");
  config.initialIntervalMs = hwmon.get<unsigned int>("-i");
  config.backlog = hwmon.get<unsigned int>("--backlog");
  return config;
}
