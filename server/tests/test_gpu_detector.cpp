#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "Devices/GpuDetector.hpp"

namespace fs = std::filesystem;

namespace {

class GpuDetectorTest : public ::testing::Test {
protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    root = fs::temp_directory_path() / ("gpu_detector_test_" + std::to_string(::getpid()) + "_" +
                                       std::to_string(counter.fetch_add(1)));
    drmRoot = root / "drm";
    devicesRoot = root / "devices";
    fs::create_directories(drmRoot);
    fs::create_directories(devicesRoot);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root, ec);
  }

  void writeFile(const fs::path &path, const std::string &contents) {
    std::ofstream f{path, std::ios::trunc};
    f << contents;
  }

  // mirrors the real layout: cardN/device is a symlink into /sys/devices, and
  // cardN/device/driver is a symlink to the bound driver
  void createCard(const std::string &cardName, const std::string &pciAddress,
                  const std::string &vendor, const std::string &device, const std::string &driver,
                  bool withHwmon = true) {
    const fs::path devicePath = devicesRoot / pciAddress;
    fs::create_directories(devicePath);
    writeFile(devicePath / "vendor", vendor + "\n");
    writeFile(devicePath / "device", device + "\n");

    const fs::path driverPath = root / "drivers" / driver;
    fs::create_directories(driverPath);
    fs::create_directory_symlink(driverPath, devicePath / "driver");

    if (withHwmon) {
      fs::create_directories(devicePath / "hwmon" / "hwmon7");
      writeFile(devicePath / "hwmon" / "hwmon7" / "temp1_input", "42000");
    }

    fs::create_directories(drmRoot / cardName);
    fs::create_directory_symlink(devicePath, drmRoot / cardName / "device");
  }

  fs::path root;
  fs::path drmRoot;
  fs::path devicesRoot;
};

} // namespace

TEST_F(GpuDetectorTest, ParsesCardMetadata) {
  createCard("card1", "0000:08:00.0", "0x1002", "0x744c", "amdgpu");

  const auto cards = GpuDetector::detect(drmRoot);

  ASSERT_EQ(cards.size(), 1u);
  EXPECT_EQ(cards[0].vendorId, PCI_VENDOR_AMD);
  EXPECT_EQ(cards[0].deviceId, 0x744cu);
  EXPECT_EQ(cards[0].pciAddress, "0000:08:00.0");
  EXPECT_EQ(cards[0].driver, "amdgpu");
  EXPECT_EQ(cards[0].hwmonPath.filename(), "hwmon7");
}

TEST_F(GpuDetectorTest, SkipsConnectorsAndRenderNodes) {
  createCard("card0", "0000:03:00.0", "0x10de", "0x2504", "nvidia");
  fs::create_directories(drmRoot / "card0-DP-1" / "device");
  fs::create_directories(drmRoot / "renderD128" / "device");
  writeFile(drmRoot / "card0-DP-1" / "device" / "vendor", "0x10de\n");
  writeFile(drmRoot / "renderD128" / "device" / "vendor", "0x10de\n");

  const auto cards = GpuDetector::detect(drmRoot);

  ASSERT_EQ(cards.size(), 1u);
  EXPECT_EQ(cards[0].cardPath.filename(), "card0");
}

TEST_F(GpuDetectorTest, SkipsUnsupportedVendors) {
  createCard("card0", "0000:01:00.0", "0x1234", "0x0001", "someothergpu");

  EXPECT_TRUE(GpuDetector::detect(drmRoot).empty());
}

TEST_F(GpuDetectorTest, DetectsMultipleCardsSortedByCardPath) {
  createCard("card0", "0000:00:02.0", "0x8086", "0x46a6", "i915");
  createCard("card1", "0000:08:00.0", "0x1002", "0x744c", "amdgpu");

  const auto cards = GpuDetector::detect(drmRoot);

  ASSERT_EQ(cards.size(), 2u);
  EXPECT_EQ(cards[0].vendorId, PCI_VENDOR_INTEL);
  EXPECT_EQ(cards[1].vendorId, PCI_VENDOR_AMD);
}

TEST_F(GpuDetectorTest, LeavesHwmonPathEmptyWhenAbsent) {
  createCard("card0", "0000:08:00.0", "0x1002", "0x1636", "amdgpu", false);

  const auto cards = GpuDetector::detect(drmRoot);

  ASSERT_EQ(cards.size(), 1u);
  EXPECT_TRUE(cards[0].hwmonPath.empty());
}

TEST_F(GpuDetectorTest, ReturnsEmptyWhenDrmRootMissing) {
  EXPECT_TRUE(GpuDetector::detect(root / "does-not-exist").empty());
}
