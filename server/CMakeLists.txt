cmake_minimum_required(VERSION 3.30)

project(Hwmon VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB SRCS CONFIGURE_DEPENDS "*.cpp")

add_executable(Hwmon ${SRCS})