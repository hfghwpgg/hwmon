#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>

#include "Devices/CpuDevice.hpp"
#include "SensorType.hpp"

namespace fs = std::filesystem;

namespace {

class CpuDeviceTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    root = fs::temp_directory_path() / ("cpu_device_test_" + std::to_string(::getpid()) + "_" +
                                        std::to_string(counter.fetch_add(1)));
    fs::create_directories(root);
    cpuinfoPath = root / "cpuinfo";
    statPath = root / "stat";
    cpufreqPath = root / "cpufreq";
    intelRaplPath = root / "intel-rapl:0" / "energy_uj";
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root, ec);
  }

  fs::path hwmonDir(const std::string &name) {
    const fs::path path = root / name;
    fs::create_directories(path);
    return path;
  }

  void writeFile(const fs::path &path, const std::string &contents) {
    std::ofstream f{path, std::ios::trunc};
    f << contents;
  }

  static nlohmann::json findSensor(const nlohmann::json &sensors, const std::string &name) {
    if (sensors.is_array()) {
      for (const auto &s : sensors) {
        if (s.value("name", std::string{}) == name) {
          return s;
        }
      }
    } else if (sensors.is_object()) {
      for (const auto &[_, group] : sensors.items()) {
        const nlohmann::json found = findSensor(group, name);
        if (!found.empty()) {
          return found;
        }
      }
    }
    return nlohmann::json{};
  }

  CpuDevice makeDevice(std::set<fs::path> &paths) {
    return CpuDevice{paths, cpufreqPath, cpuinfoPath, statPath, intelRaplPath};
  }

  fs::path root;
  fs::path cpuinfoPath;
  fs::path statPath;
  fs::path cpufreqPath;
  fs::path intelRaplPath;
};

} // namespace

TEST_F(CpuDeviceTest, ConsumesCoretempPathByName) {
  const fs::path coretemp = hwmonDir("hwmon_coretemp0");
  writeFile(coretemp / "temp1_input", "42000");

  std::set<fs::path> paths{coretemp};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read();

  const nlohmann::json j = device.serialize();
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(DeviceType::CPU));
  ASSERT_TRUE(j.contains("sensors"));
  const nlohmann::json temp = findSensor(j["sensors"], "temp1");
  ASSERT_FALSE(temp.empty());
  EXPECT_EQ(temp["type"].get<int>(), static_cast<int>(SensorType::TEMPERATURE));
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 42.0f);
}

TEST_F(CpuDeviceTest, DiscoversPackageTempByLabel) {
  const fs::path chip = hwmonDir("hwmon0");
  writeFile(chip / "temp1_label", "Package id 0");
  writeFile(chip / "temp1_input", "45000");

  std::set<fs::path> paths{chip};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read();

  const nlohmann::json temp = findSensor(device.serialize()["sensors"], "Package id 0");
  ASSERT_FALSE(temp.empty());
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 45.0f);
}

TEST_F(CpuDeviceTest, DiscoversTdieTempByLabel) {
  const fs::path chip = hwmonDir("hwmon0");
  writeFile(chip / "temp1_label", "Tdie");
  writeFile(chip / "temp1_input", "55000");

  std::set<fs::path> paths{chip};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read();

  const nlohmann::json temp = findSensor(device.serialize()["sensors"], "Tdie");
  ASSERT_FALSE(temp.empty());
  EXPECT_FLOAT_EQ(temp["readings"]["value"].get<float>(), 55.0f);
}

TEST_F(CpuDeviceTest, SkipsNvmeHwmonPaths) {
  const fs::path nvme = hwmonDir("hwmon_nvme0");
  writeFile(nvme / "temp1_input", "40000");

  std::set<fs::path> paths{nvme};
  CpuDevice device = makeDevice(paths);
  device.initialize();

  const nlohmann::json j = device.serialize();
  EXPECT_FALSE(j.contains("sensors"));
  EXPECT_EQ(paths.size(), 1u);
}

TEST_F(CpuDeviceTest, RemovesConsumedPathsFromHwmonSet) {
  const fs::path coretemp = hwmonDir("hwmon_coretemp0");
  writeFile(coretemp / "temp1_input", "42000");

  std::set<fs::path> paths{coretemp};
  CpuDevice device = makeDevice(paths);
  device.initialize();

  EXPECT_TRUE(paths.empty());
}

TEST_F(CpuDeviceTest, GetNameReadsFromCpuinfoFile) {
  writeFile(cpuinfoPath, "processor\t: 0\nmodel name\t: Test CPU Model\n");

  const fs::path coretemp = hwmonDir("hwmon_coretemp0");
  writeFile(coretemp / "temp1_input", "42000");

  std::set<fs::path> paths{coretemp};
  CpuDevice device = makeDevice(paths);
  device.initialize();

  EXPECT_EQ(device.serialize()["name"], "Test CPU Model");
}

TEST_F(CpuDeviceTest, UtilizationProducesSampleAfterSecondRead) {
  writeFile(statPath, "cpu 50 0 0 50 0 0 0 0 0 0\n"
                      "cpu0 50 0 0 50 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // baseline

  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"]["Utilization"], "CPU");
  ASSERT_FALSE(cpu.empty());
  EXPECT_EQ(cpu["type"].get<int>(), static_cast<int>(SensorType::UTILIZATION));
  EXPECT_EQ(cpu["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FLOAT_EQ(cpu["readings"]["value"].get<float>(), 50.0f);
}

TEST_F(CpuDeviceTest, CreatesFrequencySensorsFromCpufreq) {
  const fs::path policy = cpufreqPath / "policy0";
  fs::create_directories(policy);
  writeFile(policy / "scaling_cur_freq", "2400000");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read();

  const nlohmann::json freq =
      findSensor(device.serialize()["sensors"]["Core frequency"], "CPU core 0");
  ASSERT_FALSE(freq.empty());
  EXPECT_EQ(freq["type"].get<int>(), static_cast<int>(SensorType::FREQUENCY));
  EXPECT_FLOAT_EQ(freq["readings"]["value"].get<float>(), 2400.0f);
}

TEST_F(CpuDeviceTest, ResetReadingsClearsUtilizationAggregates) {
  writeFile(statPath, "cpu 50 0 0 50 0 0 0 0 0 0\n"
                      "cpu0 50 0 0 50 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read();
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpuBefore = findSensor(device.serialize()["sensors"]["Utilization"], "CPU");
  ASSERT_EQ(cpuBefore["readings"]["times"].get<std::size_t>(), 1u);

  device.resetReadings();
  const nlohmann::json cpuAfterReset =
      findSensor(device.serialize()["sensors"]["Utilization"], "CPU");
  EXPECT_EQ(cpuAfterReset["readings"]["times"].get<std::size_t>(), 0u);
}

TEST_F(CpuDeviceTest, ResetReadingsForcesNewUtilizationBaseline) {
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // baseline
  writeFile(statPath, "cpu 300 0 0 200 0 0 0 0 0 0\n"
                      "cpu0 300 0 0 200 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpuBeforeReset =
      findSensor(device.serialize()["sensors"]["Utilization"], "CPU");
  ASSERT_EQ(cpuBeforeReset["readings"]["times"].get<std::size_t>(), 1u);

  device.resetReadings();

  // Re-establish baseline only; stale utilOld must not produce a sample.
  writeFile(statPath, "cpu 600 0 0 400 0 0 0 0 0 0\n"
                      "cpu0 600 0 0 400 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpuAfterBaseline =
      findSensor(device.serialize()["sensors"]["Utilization"], "CPU");
  EXPECT_EQ(cpuAfterBaseline["readings"]["times"].get<std::size_t>(), 0u);

  writeFile(statPath, "cpu 700 0 0 500 0 0 0 0 0 0\n"
                      "cpu0 700 0 0 500 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"]["Utilization"], "CPU");
  EXPECT_EQ(cpu["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FLOAT_EQ(cpu["readings"]["value"].get<float>(), 50.0f);
}

TEST_F(CpuDeviceTest, UsesIntelRaplWhenZenergyMissing) {
  fs::create_directories(intelRaplPath.parent_path());
  writeFile(intelRaplPath, "1000000");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // energy baseline

  writeFile(intelRaplPath, "3000000");
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  device.read();

  const nlohmann::json power =
      findSensor(device.serialize()["sensors"]["Power draw"], "Socket power draw");
  ASSERT_FALSE(power.empty());
  EXPECT_EQ(power["type"].get<int>(), static_cast<int>(SensorType::POWER));
  EXPECT_EQ(power["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FALSE(std::isnan(power["readings"]["value"].get<float>()));
  EXPECT_TRUE(std::isfinite(power["readings"]["value"].get<float>()));
  EXPECT_GT(power["readings"]["value"].get<float>(), 0.0f);
}

TEST_F(CpuDeviceTest, PrefersZenergyOverIntelRapl) {
  fs::create_directories(intelRaplPath.parent_path());
  writeFile(intelRaplPath, "1000000");

  const fs::path zenergy = hwmonDir("hwmon_zenergy0");
  writeFile(zenergy / "energy1_label", "Esocket0");
  writeFile(zenergy / "energy1_input", "1000000");

  std::set<fs::path> paths{zenergy};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // energy baseline

  writeFile(zenergy / "energy1_input", "3000000");
  writeFile(intelRaplPath, "9000000");
  std::this_thread::sleep_for(std::chrono::milliseconds{5});
  device.read();

  const nlohmann::json sensors = device.serialize()["sensors"]["Power draw"];
  const nlohmann::json zenergySensor = findSensor(sensors, "Socket 0 power draw");
  ASSERT_FALSE(zenergySensor.empty());
  EXPECT_EQ(zenergySensor["type"].get<int>(), static_cast<int>(SensorType::ENERGY));
  EXPECT_EQ(zenergySensor["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FALSE(std::isnan(zenergySensor["readings"]["value"].get<float>()));
  EXPECT_TRUE(std::isfinite(zenergySensor["readings"]["value"].get<float>()));
  EXPECT_GT(zenergySensor["readings"]["value"].get<float>(), 0.0f);

  EXPECT_TRUE(findSensor(sensors, "Socket power draw").empty());
}
