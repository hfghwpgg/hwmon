set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.30)

project(Hwmon VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)


add_executable(Hwmon
    "SensorReading.cpp"
    "Sensor.cpp"
    "EnergySensor.cpp"
    "Device.cpp"
    "UDSServer.cpp"
    "Runner.cpp"
    "main.cpp"
    )

target_link_libraries(Hwmon
  PRIVATE
    Threads::Threads
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only
)

target_compile_options(Hwmon PRIVATE
  -Wno-unknown-warning-option -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:
        -O0
        -DDEBUG
        -ggdb
    >
    $<$<CONFIG:Release>:
        -O3
        -DNDEBUG
        -flto
        -march=native
    >
)
