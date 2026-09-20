#pragma once
#include "helpers.hpp"
#include <filesystem>
#include <string>
#include <vector>

// PCI vendor ids as exposed by /sys/class/drm/cardN/device/vendor
constexpr unsigned int PCI_VENDOR_AMD = 0x1002;
constexpr unsigned int PCI_VENDOR_NVIDIA = 0x10de;
constexpr unsigned int PCI_VENDOR_INTEL = 0x8086;

struct GpuCardInfo {
  helpers::fs::path cardPath;   // /sys/class/drm/cardN
  helpers::fs::path devicePath; // /sys/class/drm/cardN/device
  helpers::fs::path hwmonPath;  // first hwmon* under device/hwmon, empty if none
  std::string pciAddress;       // 0000:08:00.0
  std::string driver;           // amdgpu, nvidia, nouveau, i915, xe
  unsigned int vendorId;
  unsigned int deviceId;
};

struct GpuDetector {
  static auto detect(const helpers::fs::path &drmRoot) -> std::vector<GpuCardInfo>;
};
