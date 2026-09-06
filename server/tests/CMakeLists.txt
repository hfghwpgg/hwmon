find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(hwmon_tests
    "test_main.cpp"
    "test_sensor_type.cpp"
    "test_helpers.cpp"
    "test_sensor_reading.cpp"
    "test_sensor.cpp"
    "test_value_sensor.cpp"
    "test_sysfs_device.cpp"
    "test_shared_hwmon_parser.cpp"
    "test_cpu_device.cpp"
    "test_gpu_detector.cpp"
    "test_amd_gpu_device.cpp"
    "test_runner.cpp"
    "test_uds_server.cpp"
)

target_link_libraries(hwmon_tests
  PRIVATE
    HwmonCore
    GTest::gtest
)

gtest_discover_tests(hwmon_tests)
