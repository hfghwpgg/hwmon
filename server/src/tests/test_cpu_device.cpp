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

#include "../Devices/CpuDevice.hpp"
#include "../Sensor/SensorType.hpp"

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

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"], "CPU");
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

  const nlohmann::json freq = findSensor(device.serialize()["sensors"], "CPU core 0");
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

  const nlohmann::json cpuBefore = findSensor(device.serialize()["sensors"], "CPU");
  ASSERT_EQ(cpuBefore["readings"]["times"].get<std::size_t>(), 1u);

  device.resetReadings();
  const nlohmann::json cpuAfterReset = findSensor(device.serialize()["sensors"], "CPU");
  EXPECT_EQ(cpuAfterReset["readings"]["times"].get<std::size_t>(), 0u);
}

// resetReadings clears the reported statistics but keeps the /proc/stat
// counters, so no sampling interval is lost across a reset
TEST_F(CpuDeviceTest, ResetReadingsKeepsUtilizationBaseline) {
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // baseline
  writeFile(statPath, "cpu 300 0 0 200 0 0 0 0 0 0\n"
                      "cpu0 300 0 0 200 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpuBeforeReset = findSensor(device.serialize()["sensors"], "CPU");
  ASSERT_EQ(cpuBeforeReset["readings"]["times"].get<std::size_t>(), 1u);

  device.resetReadings();

  const nlohmann::json cleared = findSensor(device.serialize()["sensors"], "CPU");
  ASSERT_EQ(cleared["readings"]["times"].get<std::size_t>(), 0u);

  // 500 busy of 800 total since the pre-reset sample
  writeFile(statPath, "cpu 600 0 0 400 0 0 0 0 0 0\n"
                      "cpu0 600 0 0 400 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpuAfterReset = findSensor(device.serialize()["sensors"], "CPU");
  EXPECT_EQ(cpuAfterReset["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FLOAT_EQ(cpuAfterReset["readings"]["value"].get<float>(), 60.0f);

  writeFile(statPath, "cpu 700 0 0 500 0 0 0 0 0 0\n"
                      "cpu0 700 0 0 500 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"], "CPU");
  EXPECT_EQ(cpu["readings"]["times"].get<std::size_t>(), 2u);
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

  const nlohmann::json power = findSensor(device.serialize()["sensors"], "Socket power draw");
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

  const nlohmann::json sensors = device.serialize()["sensors"];
  const nlohmann::json zenergySensor = findSensor(sensors, "Socket 0 power draw");
  ASSERT_FALSE(zenergySensor.empty());
  EXPECT_EQ(zenergySensor["type"].get<int>(), static_cast<int>(SensorType::POWER));
  EXPECT_EQ(zenergySensor["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FALSE(std::isnan(zenergySensor["readings"]["value"].get<float>()));
  EXPECT_TRUE(std::isfinite(zenergySensor["readings"]["value"].get<float>()));
  EXPECT_GT(zenergySensor["readings"]["value"].get<float>(), 0.0f);

  EXPECT_TRUE(findSensor(sensors, "Socket power draw").empty());
}

// -------------------------------------------------------------------------
// Degenerate /proc/stat states. Utilization is a delta over cpu time, so any
// interval where the counters don't advance has no answer; the device must
// report nothing for that cycle instead of a bogus sample, and must keep
// working afterwards.
// -------------------------------------------------------------------------

// initialize() only builds the sensor map, so the first read() establishes the
// counter baseline and yields nothing
TEST_F(CpuDeviceTest, UtilizationFirstReadOnlyEstablishesBaseline) {
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  ASSERT_NO_THROW(device.read());

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"], "CPU");
  ASSERT_FALSE(cpu.empty());
  EXPECT_EQ(cpu["readings"]["times"].get<std::size_t>(), 0u);
  EXPECT_TRUE(std::isnan(cpu["readings"]["value"].get<double>()));
}

// /proc/stat has USER_HZ granularity (10ms), so two polls inside the same tick
// see identical counters; the resulting 0/0 must not become a sample
TEST_F(CpuDeviceTest, UtilizationSkipsSampleWhenCountersDoNotAdvance) {
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read();                  // baseline
  ASSERT_NO_THROW(device.read()); // same tick: zero delta

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"], "CPU");
  ASSERT_FALSE(cpu.empty());
  EXPECT_EQ(cpu["readings"]["times"].get<std::size_t>(), 0u);
  EXPECT_TRUE(std::isnan(cpu["readings"]["value"].get<double>()));
}

TEST_F(CpuDeviceTest, UtilizationRecoversAfterStalledCounters) {
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // baseline (200 total, 100 idle)
  device.read(); // stalled, no sample

  // 200 total / 100 idle since the baseline
  writeFile(statPath, "cpu 200 0 0 200 0 0 0 0 0 0\n"
                      "cpu0 200 0 0 200 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json afterRecovery = findSensor(device.serialize()["sensors"], "CPU");
  EXPECT_EQ(afterRecovery["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FLOAT_EQ(afterRecovery["readings"]["value"].get<float>(), 50.0f);

  // a stall in the middle of a run must not corrupt the baseline either
  device.read(); // counters unchanged again
  const nlohmann::json afterStall = findSensor(device.serialize()["sensors"], "CPU");
  EXPECT_EQ(afterStall["readings"]["times"].get<std::size_t>(), 1u);

  // 400 total / 100 idle since the last accepted sample
  writeFile(statPath, "cpu 500 0 0 300 0 0 0 0 0 0\n"
                      "cpu0 500 0 0 300 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json resumed = findSensor(device.serialize()["sensors"], "CPU");
  EXPECT_EQ(resumed["readings"]["times"].get<std::size_t>(), 2u);
  EXPECT_FLOAT_EQ(resumed["readings"]["value"].get<float>(), 75.0f);
}

// 0% is a real reading, not a missing one: it must be recorded
TEST_F(CpuDeviceTest, UtilizationReportsZeroWhenFullyIdle) {
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // baseline

  // only the idle column advances
  writeFile(statPath, "cpu 100 0 0 200 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 200 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"], "CPU");
  EXPECT_EQ(cpu["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FLOAT_EQ(cpu["readings"]["value"].get<float>(), 0.0f);
}

TEST_F(CpuDeviceTest, UtilizationReportsHundredWhenFullyBusy) {
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // baseline

  // only a non-idle column advances
  writeFile(statPath, "cpu 200 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 200 0 0 100 0 0 0 0 0 0\n");
  device.read();

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"], "CPU");
  EXPECT_EQ(cpu["readings"]["times"].get<std::size_t>(), 1u);
  EXPECT_FLOAT_EQ(cpu["readings"]["value"].get<float>(), 100.0f);
}

// a core going offline stops appearing in /proc/stat
TEST_F(CpuDeviceTest, UtilizationSurvivesCoreDisappearingFromStat) {
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu1 100 0 0 100 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // baseline

  writeFile(statPath, "cpu 200 0 0 200 0 0 0 0 0 0\n"
                      "cpu0 200 0 0 200 0 0 0 0 0 0\n"
                      "cpu1 200 0 0 200 0 0 0 0 0 0\n");
  device.read();
  ASSERT_EQ(findSensor(device.serialize()["sensors"], "CPU core 1")["readings"]["times"]
                .get<std::size_t>(),
            1u);

  // cpu1 went offline
  writeFile(statPath, "cpu 400 0 0 300 0 0 0 0 0 0\n"
                      "cpu0 400 0 0 300 0 0 0 0 0 0\n");
  ASSERT_NO_THROW(device.read());

  const nlohmann::json sensors = device.serialize()["sensors"];
  // the surviving cores keep sampling
  EXPECT_EQ(findSensor(sensors, "CPU core 0")["readings"]["times"].get<std::size_t>(), 2u);
  // the offline core keeps its last sample and gains no new one
  const nlohmann::json gone = findSensor(sensors, "CPU core 1");
  ASSERT_FALSE(gone.empty());
  EXPECT_EQ(gone["readings"]["times"].get<std::size_t>(), 1u);
}

// current behaviour: a core appearing after initialize() is fatal, because the
// sensor map is built once and readUtilization refuses to guess
TEST_F(CpuDeviceTest, UtilizationThrowsWhenNewCoreAppearsAfterInit) {
  writeFile(statPath, "cpu 100 0 0 100 0 0 0 0 0 0\n"
                      "cpu0 100 0 0 100 0 0 0 0 0 0\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  device.initialize();
  device.read(); // baseline

  writeFile(statPath, "cpu 200 0 0 200 0 0 0 0 0 0\n"
                      "cpu0 200 0 0 200 0 0 0 0 0 0\n"
                      "cpu1 200 0 0 200 0 0 0 0 0 0\n");

  EXPECT_THROW(device.read(), std::runtime_error);
}

TEST_F(CpuDeviceTest, UtilizationSurvivesMalformedStatColumns) {
  writeFile(statPath, "cpu bogus columns here\n"
                      "cpu0 bogus columns here\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  ASSERT_NO_THROW(device.initialize());
  ASSERT_NO_THROW(device.read());
  ASSERT_NO_THROW(device.read());

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"], "CPU");
  ASSERT_FALSE(cpu.empty());
  EXPECT_EQ(cpu["readings"]["times"].get<std::size_t>(), 0u);
  EXPECT_TRUE(std::isnan(cpu["readings"]["value"].get<double>()));
}

TEST_F(CpuDeviceTest, SurvivesStatWithoutCpuLines) {
  writeFile(statPath, "intr 12345\n"
                      "ctxt 678\n");

  std::set<fs::path> paths{};
  CpuDevice device = makeDevice(paths);
  ASSERT_NO_THROW(device.initialize());
  ASSERT_NO_THROW(device.read());
  EXPECT_TRUE(findSensor(device.serialize()["sensors"], "CPU").empty());
}
