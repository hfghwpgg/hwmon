#include "GpuDetector.hpp"

#include <algorithm>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <string>
#include <system_error>
#include <vector>

#include "../helpers.hpp"

namespace fs = std::filesystem;

namespace {

// cardN only; skips connectors (card1-DP-1) and render nodes (renderD128)
bool isCardNode(const std::string &filename) {
  if (!filename.starts_with("card") || filename.size() <= 4)
    return false;
  return std::all_of(filename.begin() + 4, filename.end(),
                     [](char c) { return c >= '0' && c <= '9'; });
}

fs::path findHwmon(const fs::path &devicePath) {
  const auto hwmonRoot = devicePath / "hwmon";
  std::error_code ec;
  if (!fs::is_directory(hwmonRoot, ec))
    return {};
  for (const auto &entry : fs::directory_iterator(hwmonRoot, ec)) {
    if (entry.is_directory(ec))
      return fs::canonical(entry.path(), ec);
  }
  return {};
}

// sysfs id nodes hold values like "0x1002\n"; base 0 autodetects the prefix
unsigned int readHexId(const fs::path &path) {
  const std::string raw = helpers::readFileFirstLine(path);
  try {
    return static_cast<unsigned int>(std::stoul(raw, nullptr, 0));
  } catch (const std::exception &) {
    return 0;
  }
}

} // namespace

std::vector<GpuCardInfo> GpuDetector::detect(const fs::path &drmRoot) {
  std::vector<GpuCardInfo> cards;

  std::error_code ec;
  if (!fs::is_directory(drmRoot, ec)) {
    spdlog::debug("{} is not a directory, no GPUs will be detected", drmRoot.string());
    return cards;
  }

  for (const auto &entry : fs::directory_iterator(drmRoot, ec)) {
    if (!isCardNode(entry.path().filename().string()))
      continue;

    const auto devicePath = entry.path() / "device";
    if (!fs::exists(devicePath / "vendor"))
      continue;

    GpuCardInfo card{};
    card.cardPath = entry.path();
    card.devicePath = devicePath;
    card.vendorId = readHexId(devicePath / "vendor");
    card.deviceId = readHexId(devicePath / "device");

    if (card.vendorId != PCI_VENDOR_AMD && card.vendorId != PCI_VENDOR_NVIDIA &&
        card.vendorId != PCI_VENDOR_INTEL) {
      spdlog::debug("{}: unsupported gpu vendor {:#06x}", entry.path().string(), card.vendorId);
      continue;
    }

    // device is a symlink into /sys/devices/..., its name is the PCI address
    const auto deviceTarget = fs::read_symlink(devicePath, ec);
    if (!ec)
      card.pciAddress = deviceTarget.filename().string();

    const auto driverTarget = fs::read_symlink(devicePath / "driver", ec);
    if (!ec)
      card.driver = driverTarget.filename().string();

    card.hwmonPath = findHwmon(devicePath);

    spdlog::debug("detected gpu {} vendor {:#06x} device {:#06x} driver {}", card.pciAddress,
                  card.vendorId, card.deviceId, card.driver);
    cards.push_back(std::move(card));
  }

  std::ranges::sort(cards, [](const GpuCardInfo &a, const GpuCardInfo &b) {
    return a.cardPath < b.cardPath;
  });
  return cards;
}
