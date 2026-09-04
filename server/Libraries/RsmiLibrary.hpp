#pragma once
#include <cstdint>
#include <memory>
#include <string>

// ROCm SMI defines, structs & typedefs, ported from btop so that no ROCm
// headers are needed at build time; the library is resolved with dlopen.
#define RSMI_DEVICE_NAME_BUFFER_SIZE 128
#define RSMI_MAX_NUM_FREQUENCIES_V5 32
#define RSMI_MAX_NUM_FREQUENCIES_V6 33
#define RSMI_STATUS_SUCCESS 0
#define RSMI_MEM_TYPE_VRAM 0
#define RSMI_TEMP_CURRENT 0
#define RSMI_TEMP_MAX 1
#define RSMI_TEMP_TYPE_EDGE 0
#define RSMI_TEMP_TYPE_JUNCTION 1
#define RSMI_TEMP_TYPE_MEMORY 2
#define RSMI_CLK_TYPE_SYS 0
#define RSMI_CLK_TYPE_MEM 4

using rsmi_status_t = int;
using rsmi_temperature_metric_t = int;
using rsmi_clk_type_t = int;
using rsmi_memory_type_t = int;

struct rsmi_version_t {
  uint32_t major, minor, patch;
  const char *build;
};

struct rsmi_frequencies_t_v5 {
  uint32_t num_supported, current;
  uint64_t frequency[RSMI_MAX_NUM_FREQUENCIES_V5];
};

struct rsmi_frequencies_t_v6 {
  bool has_deep_sleep;
  uint32_t num_supported, current;
  uint64_t frequency[RSMI_MAX_NUM_FREQUENCIES_V6];
};

class RsmiLibrary {
public:
  // returns nullptr when librocm_smi64 is unavailable or reports no devices;
  // the library stays loaded as long as any caller holds the handle
  static std::shared_ptr<RsmiLibrary> acquire();

  RsmiLibrary(const RsmiLibrary &) = delete;
  RsmiLibrary &operator=(const RsmiLibrary &) = delete;
  ~RsmiLibrary();

  // ROCm SMI indexes devices in its own order, so cards are matched by PCI address
  bool findIndexByPciAddress(const std::string &pciAddress, uint32_t &index);

  // hides the v5/v6 struct layout difference from callers; returns the
  // current frequency in MHz, or -1 when unsupported
  long long getCurrentClockMhz(uint32_t index, rsmi_clk_type_t clockType);

  uint32_t deviceCount() const { return device_count; }

  //? Function pointers
  rsmi_status_t (*rsmi_init)(uint64_t) = nullptr;
  rsmi_status_t (*rsmi_shut_down)() = nullptr;
  rsmi_status_t (*rsmi_version_get)(rsmi_version_t *) = nullptr;
  rsmi_status_t (*rsmi_num_monitor_devices)(uint32_t *) = nullptr;
  rsmi_status_t (*rsmi_dev_name_get)(uint32_t, char *, size_t) = nullptr;
  rsmi_status_t (*rsmi_dev_pci_id_get)(uint32_t, uint64_t *) = nullptr;
  rsmi_status_t (*rsmi_dev_power_cap_get)(uint32_t, uint32_t, uint64_t *) = nullptr;
  rsmi_status_t (*rsmi_dev_temp_metric_get)(uint32_t, uint32_t, rsmi_temperature_metric_t,
                                            int64_t *) = nullptr;
  rsmi_status_t (*rsmi_dev_busy_percent_get)(uint32_t, uint32_t *) = nullptr;
  rsmi_status_t (*rsmi_dev_memory_busy_percent_get)(uint32_t, uint32_t *) = nullptr;
  rsmi_status_t (*rsmi_dev_gpu_clk_freq_get_v5)(uint32_t, rsmi_clk_type_t,
                                                rsmi_frequencies_t_v5 *) = nullptr;
  rsmi_status_t (*rsmi_dev_gpu_clk_freq_get_v6)(uint32_t, rsmi_clk_type_t,
                                                rsmi_frequencies_t_v6 *) = nullptr;
  rsmi_status_t (*rsmi_dev_power_ave_get)(uint32_t, uint32_t, uint64_t *) = nullptr;
  rsmi_status_t (*rsmi_dev_memory_total_get)(uint32_t, rsmi_memory_type_t, uint64_t *) = nullptr;
  rsmi_status_t (*rsmi_dev_memory_usage_get)(uint32_t, rsmi_memory_type_t, uint64_t *) = nullptr;
  rsmi_status_t (*rsmi_dev_pci_throughput_get)(uint32_t, uint64_t *, uint64_t *,
                                               uint64_t *) = nullptr;

private:
  RsmiLibrary() = default;

  bool load();

  void *dlHandle = nullptr;
  bool initialized = false;
  uint32_t device_count = 0;
  uint32_t version_major = 0;
};
