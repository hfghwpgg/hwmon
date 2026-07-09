#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
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
    for (const auto &s : sensors) {
      if (s.value("name", std::string{}) == name) {
        return s;
      }
    }
    return nlohmann::json{};
  }

  CpuDevice makeDevice(std::set<fs::path> &paths) {
    return CpuDevice{paths, cpufreqPath, cpuinfoPath, statPath};
  }

  fs::path root;
  fs::path cpuinfoPath;
  fs::path statPath;
  fs::path cpufreqPath;
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

  const nlohmann::json cpu = findSensor(device.serialize()["sensors"], "cpu");
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

  const nlohmann::json freq = findSensor(device.serialize()["sensors"], "cpu0");
  ASSERT_FALSE(freq.empty());
  EXPECT_EQ(freq["type"].get<int>(), static_cast<int>(SensorType::FREQUENCY));
  EXPECT_FLOAT_EQ(freq["readings"]["value"].get<float>(), 2400.0f);
}
