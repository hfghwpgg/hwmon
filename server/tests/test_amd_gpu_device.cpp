#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <unistd.h>

#include "Device.hpp"
#include "Devices/AmdGpuDevice.hpp"
#include "Devices/GpuDetector.hpp"
#include "SensorType.hpp"

namespace fs = std::filesystem;

namespace {

// exercises the sysfs backend only; the ROCm SMI path needs real hardware
class AmdGpuDeviceTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    root = fs::temp_directory_path() / ("amd_gpu_device_test_" + std::to_string(::getpid()) + "_" +
                                       std::to_string(counter.fetch_add(1)));
    devicePath = root / "0000:08:00.0";
    hwmonPath = devicePath / "hwmon" / "hwmon3";
    fs::create_directories(hwmonPath);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root, ec);
  }

  void writeFile(const fs::path &path, const std::string &contents) {
    std::ofstream f{path, std::ios::trunc};
    f << contents;
  }

  GpuCardInfo card() {
    return GpuCardInfo{.cardPath = root / "card1",
                       .devicePath = devicePath,
                       .hwmonPath = hwmonPath,
                       .pciAddress = "0000:08:00.0",
                       .driver = "amdgpu",
                       .vendorId = PCI_VENDOR_AMD,
                       .deviceId = 0x744c};
  }

  static nlohmann::json findSensor(const nlohmann::json &sensors, const std::string &name) {
    for (const auto &s : sensors) {
      if (s.value("name", std::string{}) == name)
        return s;
    }
    return nlohmann::json{};
  }

  fs::path root;
  fs::path devicePath;
  fs::path hwmonPath;
};

} // namespace

TEST_F(AmdGpuDeviceTest, ReadsUtilizationAndVramFromSysfs) {
  writeFile(devicePath / "gpu_busy_percent", "37");
  writeFile(devicePath / "mem_busy_percent", "12");
  writeFile(devicePath / "mem_info_vram_total", "25753026560");
  writeFile(devicePath / "mem_info_vram_used", "1073741824");

  std::set<fs::path> hwmonPaths{hwmonPath};
  AmdGpuDevice device{card(), hwmonPaths, false};
  device.initialize();
  device.read();

  const nlohmann::json j = device.serialize();
  EXPECT_EQ(j["type"].get<int>(), static_cast<int>(DeviceType::GPU));
  EXPECT_EQ(j["name"], "AMD GPU (1002:744c)");

  const nlohmann::json busy = findSensor(j["sensors"], "gpu_busy");
  ASSERT_FALSE(busy.empty());
  EXPECT_EQ(busy["type"].get<int>(), static_cast<int>(SensorType::UTILIZATION));
  EXPECT_DOUBLE_EQ(busy["readings"]["value"].get<double>(), 37.0);

  EXPECT_DOUBLE_EQ(findSensor(j["sensors"], "mem_busy")["readings"]["value"].get<double>(), 12.0);

  const nlohmann::json vram = findSensor(j["sensors"], "vram_used");
  ASSERT_FALSE(vram.empty());
  EXPECT_EQ(vram["type"].get<int>(), static_cast<int>(SensorType::MEMORY));
  EXPECT_DOUBLE_EQ(vram["readings"]["value"].get<double>(), 1073741824.0);
}

TEST_F(AmdGpuDeviceTest, ReadsHwmonSensorsOfTheCard) {
  writeFile(hwmonPath / "temp1_label", "edge");
  writeFile(hwmonPath / "temp1_input", "48000");
  writeFile(hwmonPath / "fan1_input", "1200");
  writeFile(hwmonPath / "power1_average", "35000000");
  writeFile(hwmonPath / "freq1_label", "sclk");
  writeFile(hwmonPath / "freq1_input", "2400000000");

  std::set<fs::path> hwmonPaths{hwmonPath};
  AmdGpuDevice device{card(), hwmonPaths, false};
  device.initialize();
  device.read();

  const nlohmann::json sensors = device.serialize()["sensors"];
  EXPECT_DOUBLE_EQ(findSensor(sensors, "edge")["readings"]["value"].get<double>(), 48.0);
  EXPECT_DOUBLE_EQ(findSensor(sensors, "fan1")["readings"]["value"].get<double>(), 1200.0);
  EXPECT_DOUBLE_EQ(findSensor(sensors, "power1")["readings"]["value"].get<double>(), 35.0);
  EXPECT_DOUBLE_EQ(findSensor(sensors, "sclk")["readings"]["value"].get<double>(), 2400.0);
}

TEST_F(AmdGpuDeviceTest, ClaimsItsHwmonPath) {
  writeFile(hwmonPath / "temp1_input", "48000");

  std::set<fs::path> hwmonPaths{hwmonPath};
  AmdGpuDevice device{card(), hwmonPaths, false};
  device.initialize();

  EXPECT_TRUE(hwmonPaths.empty());
}

TEST_F(AmdGpuDeviceTest, SurvivesCardWithoutAnyMetrics) {
  GpuCardInfo bare = card();
  bare.hwmonPath.clear();

  std::set<fs::path> hwmonPaths{};
  AmdGpuDevice device{bare, hwmonPaths, false};
  device.initialize();
  device.read();

  EXPECT_FALSE(device.serialize().contains("sensors"));
}
