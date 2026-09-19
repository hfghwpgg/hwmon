#pragma once
#include <memory>
#include <string>

// NVML defines, structs & typedefs, ported from btop so that no NVIDIA headers
// are needed at build time; the library is resolved with dlopen at runtime.
#define NVML_DEVICE_NAME_BUFFER_SIZE 64
#define NVML_SUCCESS 0
#define NVML_TEMPERATURE_THRESHOLD_SHUTDOWN 0
#define NVML_CLOCK_GRAPHICS 0
#define NVML_CLOCK_MEM 2
#define NVML_TEMPERATURE_GPU 0
#define NVML_PCIE_UTIL_TX_BYTES 0
#define NVML_PCIE_UTIL_RX_BYTES 1

// we never access the underlying struct's properties, so an opaque pointer is fine
using nvmlDevice_t = void *;
// enums are basically ints
using nvmlReturn_t = int;
using nvmlTemperatureThresholds_t = int;
using nvmlClockType_t = int;
using nvmlPstates_t = int;
using nvmlTemperatureSensors_t = int;
using nvmlPcieUtilCounter_t = int;

struct nvmlUtilization_t {
  unsigned int gpu, memory;
};

struct nvmlMemory_t {
  unsigned long long total, free, used;
};

class NvmlLibrary {
public:
  // returns nullptr when libnvidia-ml is unavailable or reports no devices;
  // the library stays loaded as long as any caller holds the handle
  static std::shared_ptr<NvmlLibrary> acquire();

  NvmlLibrary(const NvmlLibrary &) = delete;
  NvmlLibrary &operator=(const NvmlLibrary &) = delete;
  ~NvmlLibrary();

  // NVML indexes devices in its own order, so cards are matched by PCI address
  bool getHandleByPciAddress(const std::string &pciAddress, nvmlDevice_t &handle);
  unsigned int deviceCount() const { return device_count; }
  const char *errorString(nvmlReturn_t result) const { return nvmlErrorString(result); }

  //? Function pointers
  const char *(*nvmlErrorString)(nvmlReturn_t) = nullptr;
  nvmlReturn_t (*nvmlInit)() = nullptr;
  nvmlReturn_t (*nvmlShutdown)() = nullptr;
  nvmlReturn_t (*nvmlDeviceGetCount)(unsigned int *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetHandleByPciBusId)(const char *, nvmlDevice_t *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetName)(nvmlDevice_t, char *, unsigned int) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetPowerManagementLimit)(nvmlDevice_t, unsigned int *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetTemperatureThreshold)(nvmlDevice_t, nvmlTemperatureThresholds_t,
                                                    unsigned int *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetClockInfo)(nvmlDevice_t, nvmlClockType_t, unsigned int *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetPowerState)(nvmlDevice_t, nvmlPstates_t *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetTemperature)(nvmlDevice_t, nvmlTemperatureSensors_t,
                                           unsigned int *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetPcieThroughput)(nvmlDevice_t, nvmlPcieUtilCounter_t,
                                              unsigned int *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetEncoderUtilization)(nvmlDevice_t, unsigned int *,
                                                  unsigned int *) = nullptr;
  nvmlReturn_t (*nvmlDeviceGetDecoderUtilization)(nvmlDevice_t, unsigned int *,
                                                  unsigned int *) = nullptr;

private:
  NvmlLibrary() = default;

  bool load();

  void *dlHandle = nullptr;
  bool initialized = false;
  unsigned int device_count = 0;
};
