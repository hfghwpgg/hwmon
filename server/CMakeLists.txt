set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.30)

project(Hwmon VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
file(GLOB SRCS CONFIGURE_DEPENDS "*.cpp")

add_executable(Hwmon ${SRCS})

target_compile_options(Hwmon PRIVATE
  -Wno-unknown-warning-option -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:
        -O0
        -g
    >
    $<$<CONFIG:Release>:
        -O3
        -DNDEBUG
        -flto
        -march=native
    >
)
