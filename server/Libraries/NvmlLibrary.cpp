#include "NvmlLibrary.hpp"

#include <array>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

#ifndef HWMON_NO_DLOPEN
#include <dlfcn.h>
#endif

namespace {
std::weak_ptr<NvmlLibrary> instance;
} // namespace

std::shared_ptr<NvmlLibrary> NvmlLibrary::acquire() {
  if (auto existing = instance.lock())
    return existing;

#ifdef HWMON_NO_DLOPEN
  spdlog::debug("NVML: dynamic loading disabled in this build");
  return nullptr;
#else
  auto library = std::shared_ptr<NvmlLibrary>(new NvmlLibrary());
  if (!library->load())
    return nullptr;

  instance = library;
  return library;
#endif
}

NvmlLibrary::~NvmlLibrary() {
#ifndef HWMON_NO_DLOPEN
  if (initialized && nvmlShutdown() != NVML_SUCCESS)
    spdlog::warn("NVML: failed to shut down");
  if (dlHandle != nullptr)
    dlclose(dlHandle);
#endif
}

#ifdef HWMON_NO_DLOPEN

bool NvmlLibrary::load() {
  return false;
}

#else

bool NvmlLibrary::load() {
  const std::array libAlts = {
      "libnvidia-ml.so",
      "libnvidia-ml.so.1",
  };

  for (const auto &lib : libAlts) {
    dlHandle = dlopen(lib, RTLD_LAZY);
    if (dlHandle != nullptr)
      break;
  }
  if (dlHandle == nullptr) {
    spdlog::info("NVML: failed to load libnvidia-ml.so, NVIDIA GPUs will use sysfs only: {}",
                 dlerror());
    return false;
  }

  auto loadSymbol = [this](const char *symbolName) -> void * {
    void *symbol = dlsym(dlHandle, symbolName);
    const char *error = dlerror();
    if (error != nullptr) {
      spdlog::error("NVML: couldn't find function {}: {}", symbolName, error);
      return nullptr;
    }
    return symbol;
  };

#define LOAD_SYM(NAME)                                                                             \
  if ((NAME = reinterpret_cast<decltype(NAME)>(loadSymbol(#NAME))) == nullptr)                     \
  return false

  LOAD_SYM(nvmlErrorString);
  LOAD_SYM(nvmlInit);
  LOAD_SYM(nvmlShutdown);
  LOAD_SYM(nvmlDeviceGetCount);
  LOAD_SYM(nvmlDeviceGetHandleByIndex);
  LOAD_SYM(nvmlDeviceGetName);
  LOAD_SYM(nvmlDeviceGetPowerManagementLimit);
  LOAD_SYM(nvmlDeviceGetTemperatureThreshold);
  LOAD_SYM(nvmlDeviceGetUtilizationRates);
  LOAD_SYM(nvmlDeviceGetClockInfo);
  LOAD_SYM(nvmlDeviceGetPowerUsage);
  LOAD_SYM(nvmlDeviceGetPowerState);
  LOAD_SYM(nvmlDeviceGetTemperature);
  LOAD_SYM(nvmlDeviceGetMemoryInfo);
  LOAD_SYM(nvmlDeviceGetPcieThroughput);
  LOAD_SYM(nvmlDeviceGetEncoderUtilization);
  LOAD_SYM(nvmlDeviceGetDecoderUtilization);

#undef LOAD_SYM

  // the _v2 variant accepts the full domain:bus:device.function form
  nvmlDeviceGetHandleByPciBusId = reinterpret_cast<decltype(nvmlDeviceGetHandleByPciBusId)>(
      dlsym(dlHandle, "nvmlDeviceGetHandleByPciBusId_v2"));
  (void)dlerror();
  if (nvmlDeviceGetHandleByPciBusId == nullptr) {
    nvmlDeviceGetHandleByPciBusId = reinterpret_cast<decltype(nvmlDeviceGetHandleByPciBusId)>(
        dlsym(dlHandle, "nvmlDeviceGetHandleByPciBusId"));
    (void)dlerror();
  }

  nvmlReturn_t result = nvmlInit();
  if (result != NVML_SUCCESS) {
    spdlog::debug("NVML: failed to initialize: {}", nvmlErrorString(result));
    return false;
  }
  initialized = true;

  result = nvmlDeviceGetCount(&device_count);
  if (result != NVML_SUCCESS) {
    spdlog::warn("NVML: failed to get device count: {}", nvmlErrorString(result));
    return false;
  }

  if (device_count == 0) {
    spdlog::debug("NVML: no devices reported");
    return false;
  }

  spdlog::info("NVML: initialized, {} device(s)", device_count);
  return true;
}

#endif // HWMON_NO_DLOPEN

bool NvmlLibrary::getHandleByPciAddress(const std::string &pciAddress, nvmlDevice_t &handle) {
  if (!initialized)
    return false;

  if (nvmlDeviceGetHandleByPciBusId != nullptr && !pciAddress.empty()) {
    const nvmlReturn_t result = nvmlDeviceGetHandleByPciBusId(pciAddress.c_str(), &handle);
    if (result == NVML_SUCCESS)
      return true;
    spdlog::debug("NVML: lookup by PCI address {} failed: {}", pciAddress,
                  nvmlErrorString(result));
  }

  // no usable PCI lookup: fall back to the single-GPU case, where index 0 is unambiguous
  if (device_count == 1) {
    const nvmlReturn_t result = nvmlDeviceGetHandleByIndex(0, &handle);
    if (result == NVML_SUCCESS)
      return true;
    spdlog::warn("NVML: failed to get device handle: {}", nvmlErrorString(result));
  }

  return false;
}
