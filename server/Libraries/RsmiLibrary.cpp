#include "RsmiLibrary.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

#ifndef HWMON_NO_DLOPEN
#include <dlfcn.h>
#endif

namespace {
std::weak_ptr<RsmiLibrary> instance;
} // namespace

std::shared_ptr<RsmiLibrary> RsmiLibrary::acquire() {
  if (auto existing = instance.lock())
    return existing;

#ifdef HWMON_NO_DLOPEN
  spdlog::debug("ROCm SMI: dynamic loading disabled in this build");
  return nullptr;
#else
  auto library = std::shared_ptr<RsmiLibrary>(new RsmiLibrary());
  if (!library->load())
    return nullptr;

  instance = library;
  return library;
#endif
}

RsmiLibrary::~RsmiLibrary() {
#ifndef HWMON_NO_DLOPEN
  if (initialized && rsmi_shut_down() != RSMI_STATUS_SUCCESS)
    spdlog::warn("ROCm SMI: failed to shut down");
  if (dlHandle != nullptr)
    dlclose(dlHandle);
#endif
}

#ifdef HWMON_NO_DLOPEN

bool RsmiLibrary::load() {
  return false;
}

#else

bool RsmiLibrary::load() {
  const std::array libAlts = {
      "/opt/rocm/lib/librocm_smi64.so",
      "librocm_smi64.so",
      "librocm_smi64.so.5",   // fedora
      "librocm_smi64.so.1.0", // debian
      "librocm_smi64.so.6",
      "librocm_smi64.so.7", // rocm 7 / ubuntu 26.04
  };

  for (const auto &lib : libAlts) {
    dlHandle = dlopen(lib, RTLD_LAZY);
    if (dlHandle != nullptr)
      break;
  }
  if (dlHandle == nullptr) {
    spdlog::info("ROCm SMI: failed to load librocm_smi64.so, AMD GPUs will use sysfs: {}",
                 dlerror());
    return false;
  }

  auto loadSymbol = [this](const char *symbolName) -> void * {
    void *symbol = dlsym(dlHandle, symbolName);
    const char *error = dlerror();
    if (error != nullptr) {
      spdlog::error("ROCm SMI: couldn't find function {}: {}", symbolName, error);
      return nullptr;
    }
    return symbol;
  };

#define LOAD_SYM(NAME)                                                                             \
  if ((NAME = reinterpret_cast<decltype(NAME)>(loadSymbol(#NAME))) == nullptr)                     \
  return false

  LOAD_SYM(rsmi_init);
  LOAD_SYM(rsmi_shut_down);
  LOAD_SYM(rsmi_version_get);
  LOAD_SYM(rsmi_num_monitor_devices);
  LOAD_SYM(rsmi_dev_name_get);
  LOAD_SYM(rsmi_dev_pci_id_get);
  LOAD_SYM(rsmi_dev_power_cap_get);
  LOAD_SYM(rsmi_dev_temp_metric_get);
  LOAD_SYM(rsmi_dev_busy_percent_get);
  LOAD_SYM(rsmi_dev_memory_busy_percent_get);
  LOAD_SYM(rsmi_dev_power_ave_get);
  LOAD_SYM(rsmi_dev_memory_total_get);
  LOAD_SYM(rsmi_dev_memory_usage_get);
  LOAD_SYM(rsmi_dev_pci_throughput_get);

#undef LOAD_SYM

  rsmi_status_t result = rsmi_init(0);
  if (result != RSMI_STATUS_SUCCESS) {
    spdlog::debug("ROCm SMI: failed to initialize");
    return false;
  }
  initialized = true;

  rsmi_version_t version{};
  result = rsmi_version_get(&version);
  if (result != RSMI_STATUS_SUCCESS) {
    spdlog::warn("ROCm SMI: failed to get version");
    return false;
  }

  // Two distinct real-world libraries report version.major == 1:
  //   - ROCm 7.2 ships the v6 ABI (upstream PR #1566).
  //   - Debian/Ubuntu's librocm-smi64 is built from 5.x sources but
  //     rocm_smi64Config.h reports 1.0.0, so the ABI is v5.
  // Probe a 6.x-only symbol to disambiguate instead of guessing.
  version_major = version.major;
  if (version.major == 1) {
    const bool hasV6Symbol = dlsym(dlHandle, "rsmi_dev_activity_metric_get") != nullptr;
    (void)dlerror(); // clear error state from the probe
    version_major = hasV6Symbol ? 6 : 5;
    spdlog::warn("ROCm SMI: library reports version 1.x; assuming {}.x ABI based on symbol probe",
                 version_major);
  }

  if (version_major == 5) {
    rsmi_dev_gpu_clk_freq_get_v5 = reinterpret_cast<decltype(rsmi_dev_gpu_clk_freq_get_v5)>(
        loadSymbol("rsmi_dev_gpu_clk_freq_get"));
    if (rsmi_dev_gpu_clk_freq_get_v5 == nullptr)
      return false;
    // in the release tarballs of rocm 6.0.0 and 6.0.2 rsmi_version_get reports 7.0.0.0
  } else if (version_major == 6 || version_major == 7) {
    rsmi_dev_gpu_clk_freq_get_v6 = reinterpret_cast<decltype(rsmi_dev_gpu_clk_freq_get_v6)>(
        loadSymbol("rsmi_dev_gpu_clk_freq_get"));
    if (rsmi_dev_gpu_clk_freq_get_v6 == nullptr)
      return false;
  } else {
    spdlog::warn("ROCm SMI: dynamic loading only supported for versions 5 to 7");
    return false;
  }

  result = rsmi_num_monitor_devices(&device_count);
  if (result != RSMI_STATUS_SUCCESS) {
    spdlog::warn("ROCm SMI: failed to fetch number of devices");
    return false;
  }

  if (device_count == 0) {
    spdlog::debug("ROCm SMI: no devices reported");
    return false;
  }

  spdlog::info("ROCm SMI: initialized (v{} ABI), {} device(s)", version_major, device_count);
  return true;
}

#endif // HWMON_NO_DLOPEN

bool RsmiLibrary::findIndexByPciAddress(const std::string &pciAddress, uint32_t &index) {
  if (!initialized)
    return false;

  for (uint32_t i = 0; i < device_count; ++i) {
    uint64_t bdf = 0;
    if (rsmi_dev_pci_id_get(i, &bdf) != RSMI_STATUS_SUCCESS)
      continue;

    // bdf layout: domain [63:32], bus [15:8], device [7:3], function [2:0]
    const auto candidate = std::format("{:04x}:{:02x}:{:02x}.{:x}", (bdf >> 32) & 0xffffffffULL,
                                       (bdf >> 8) & 0xffULL, (bdf >> 3) & 0x1fULL, bdf & 0x7ULL);
    if (candidate == pciAddress) {
      index = i;
      return true;
    }
  }

  spdlog::debug("ROCm SMI: no device matches PCI address {}", pciAddress);
  return false;
}

long long RsmiLibrary::getCurrentClockMhz(uint32_t index, rsmi_clk_type_t clockType) {
  if (!initialized)
    return -1;

  // Some AMD devices return RSMI_STATUS_SUCCESS but leave frequencies.current
  // uninitialized, which would crash when used as an array index. Bound-check
  // before indexing.
  if (rsmi_dev_gpu_clk_freq_get_v5 != nullptr) {
    rsmi_frequencies_t_v5 frequencies{};
    if (rsmi_dev_gpu_clk_freq_get_v5(index, clockType, &frequencies) != RSMI_STATUS_SUCCESS)
      return -1;
    if (frequencies.num_supported == 0 || frequencies.current >= frequencies.num_supported ||
        frequencies.num_supported > RSMI_MAX_NUM_FREQUENCIES_V5)
      return -1;
    return static_cast<long long>(frequencies.frequency[frequencies.current] / 1'000'000);
  }

  if (rsmi_dev_gpu_clk_freq_get_v6 != nullptr) {
    rsmi_frequencies_t_v6 frequencies{};
    if (rsmi_dev_gpu_clk_freq_get_v6(index, clockType, &frequencies) != RSMI_STATUS_SUCCESS)
      return -1;
    if (frequencies.num_supported == 0 || frequencies.current >= frequencies.num_supported ||
        frequencies.num_supported > RSMI_MAX_NUM_FREQUENCIES_V6)
      return -1;
    return static_cast<long long>(frequencies.frequency[frequencies.current] / 1'000'000);
  }

  return -1;
}
