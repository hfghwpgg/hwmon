cmake_minimum_required(VERSION 3.16)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_executable(hwmon-client
    main.cpp
    ClientBackend.cpp
    ClientBackend.hpp
    MonitorModel.cpp
    MonitorModel.hpp
    MainWindow.cpp
    MainWindow.hpp
    Types.hpp
    UiStyle.hpp
)

qt_add_resources(hwmon-client "icons"
    PREFIX "/icons"
    BASE "svg"
    FILES
        svg/clock.svg
        svg/voltage.svg
        svg/temperature.svg
        svg/fan.svg
        svg/chip.svg
)

set_target_properties(hwmon-client PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(hwmon-client
    PRIVATE Qt6::Widgets Qt6::Network Qt6::Svg
)

install(TARGETS hwmon-client
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
