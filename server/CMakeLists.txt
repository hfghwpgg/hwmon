set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.30)

project(Hwmon VERSION 0.1 LANGUAGES CXX)

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
  "$<$<CONFIG:Debug>:-O0;-ggdb>"
  "$<$<CONFIG:Release>:-O3;-flto;-march=native>"
)
target_compile_definitions(hwmon_opts INTERFACE
  "$<$<CONFIG:Release>:NDEBUG>"
)

add_library(HwmonCore STATIC
    "helpers.hpp"
    "SensorReading.cpp"
    "Sensor.cpp"
    "EnergySensor.cpp"
    "Device.cpp"
    "Devices/SharedHwmonParser.cpp"
    "Devices/GeneralDevice.cpp"
    "Devices/CpuDevice.cpp"
    "UDSServer.cpp"
    "Runner.cpp"
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

add_executable(Hwmon "main.cpp")
target_link_libraries(Hwmon PRIVATE HwmonCore hwmon_opts)

option(HWMON_BUILD_TESTS "Build the unit test suite (requires GTest)" OFF)
if(HWMON_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
