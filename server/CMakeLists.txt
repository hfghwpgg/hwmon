set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.30)

project(Hwmon VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)

option(BUILD_STATIC "Build the server statically using musl/static libs" OFF)

if(BUILD_STATIC)
  set(CMAKE_EXE_LINKER_FLAGS "-static")
  include(FetchContent)
  # spdlog
  FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.17.0
    )
  set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
  set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  # nlohmann_json
  FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.12.0
    )

  FetchContent_MakeAvailable(spdlog)
  FetchContent_MakeAvailable(nlohmann_json)
else()
  find_package(spdlog REQUIRED)
  find_package(nlohmann_json REQUIRED)
endif()

find_package(Threads REQUIRED)

add_library(hwmon_opts INTERFACE)
target_compile_options(hwmon_opts INTERFACE
  -Wall -Wextra -Wpedantic
  "$<$<CONFIG:Debug>:-Og;-ggdb>"
  "$<$<CONFIG:Release>:-Os;-flto;-march=native>"
)
target_compile_definitions(hwmon_opts INTERFACE
  "$<$<CONFIG:Release>:NDEBUG>"
)

# vendored from igt-gpu-tools, provides the i915 perf PMU helpers
add_subdirectory(Libraries/intel_gpu_top)

add_library(HwmonCore STATIC
    "helpers.hpp"
    "SensorReading.cpp"
    "Sensor.cpp"
    "EnergySensor.cpp"
    "ValueSensor.cpp"
    "Device.cpp"
    "Devices/SharedHwmonParser.cpp"
    "Devices/GeneralDevice.cpp"
    "Devices/CpuDevice.cpp"
    "Devices/GpuDetector.cpp"
    "Libraries/NvmlLibrary.cpp"
    "Libraries/RsmiLibrary.cpp"
    "Devices/AmdGpuDevice.cpp"
    "Devices/NvidiaGpuDevice.cpp"
    "Devices/IntelGpuDevice.cpp"
    "UDSServer.cpp"
    "Runner.cpp"
    $<TARGET_OBJECTS:igt>
)

target_include_directories(HwmonCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(HwmonCore
  PUBLIC
    Threads::Threads
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only
  PRIVATE
    hwmon_opts
)

target_compile_definitions(HwmonCore PUBLIC "$<$<CONFIG:Debug>:DEBUG>")

# NVML and ROCm SMI are resolved with dlopen, which a fully static binary
# cannot do; those GPU backends fall back to sysfs there
if(BUILD_STATIC)
  target_compile_definitions(HwmonCore PUBLIC HWMON_NO_DLOPEN)
else()
  target_link_libraries(HwmonCore PRIVATE ${CMAKE_DL_LIBS})
endif()

add_executable(Hwmon "main.cpp")
target_link_libraries(Hwmon PRIVATE HwmonCore hwmon_opts)

option(HWMON_BUILD_TESTS "Build the unit test suite (requires GTest)" OFF)
if(HWMON_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
