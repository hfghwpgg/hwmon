set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.30)

project(Hwmon VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)

find_program(IWYU_PATH include-what-you-use)

# if(IWYU_PATH)
#     set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH})
# endif()

# file(GLOB SRCS CONFIGURE_DEPENDS "*.cpp")
# add_executable(Hwmon ${SRCS})

add_executable(Hwmon
    "Logger.cpp"
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
    fmt::fmt
    stdc++exp
)

target_compile_options(Hwmon PRIVATE
  -Wno-unknown-warning-option -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:
        -O0
        -DDEBUG
        -g
    >
    $<$<CONFIG:Release>:
        -O3
        -DNDEBUG
        -flto
        -march=native
    >
)
