set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.30)

project(Hwmon VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option -fcoroutines -fexceptions -O0 -std=c++20")

file(GLOB SRCS CONFIGURE_DEPENDS "*.cpp")

add_executable(Hwmon ${SRCS})