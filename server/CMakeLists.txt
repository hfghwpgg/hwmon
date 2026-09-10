set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.30)

project(hwmon VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(argparse REQUIRED)
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

add_library(hwmonCore STATIC
    "helpers.cpp"
    "CliParser.cpp"
    "dropPrivileges.cpp"
    "SensorReading.cpp"
    "Sensor.cpp"
    "EnergySensor.cpp"
    "ValueSensor.cpp"
    "Device.cpp"
    "Devices/SharedHwmonParser.cpp"
    "Devices/SysfsDevice.cpp"
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

target_include_directories(hwmonCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(hwmonCore
  PUBLIC
    Threads::Threads
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only
  PRIVATE
    hwmon_opts
)

target_compile_definitions(hwmonCore PUBLIC "$<$<CONFIG:Debug>:DEBUG>")
target_link_libraries(hwmonCore PRIVATE ${CMAKE_DL_LIBS})

add_executable(hwmon "main.cpp")
target_link_libraries(hwmon PRIVATE hwmonCore hwmon_opts)

option(HWMON_BUILD_TESTS "Build the unit test suite (requires GTest)" OFF)
if(HWMON_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
