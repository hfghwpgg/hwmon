find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(hwmon_tests
    "test_main.cpp"
    "test_sensor_type.cpp"
    "test_helpers.cpp"
    "test_sensor_reading.cpp"
    "test_sensor.cpp"
    "test_device.cpp"
    "test_uds_server.cpp"
)

target_link_libraries(hwmon_tests
  PRIVATE
    HwmonCore
    GTest::gtest
)

gtest_discover_tests(hwmon_tests)
