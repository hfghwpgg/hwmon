#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <format>
#include "Device.hpp"
#include "Sensor.hpp"
#include "SensorTypes.hpp"
#include "SensorReading.hpp"
#include "Runner.hpp"

int main()
{
    std::cout << "hello world:" << std::endl;
    // test();
    auto d = new Device("/sys/class/hwmon/hwmon7", "asd");
    d->Display();
    delete d;


    
    // auto s = std::make_unique<Sensor>(
    //     "/sys/class/hwmon/hwmon3/temp1_input",
    //     "test",
    //     SensorTypes::VOLTAGE
    // );
    // for (int i = 0; i< 3; i++) {
    //     s->updateValue();
    //     auto read = s->getReadings();
    //     std::cout << std::format(" val: {}\n min: {}\n max: {}\n avg: {}\n", read.value, read.min_value, read.max_value, read.sum / read.times);
    //     std::this_thread::sleep_for(std::chrono::seconds{1});
    // }

    // auto runner = std::make_unique<Runner>();



    return 0;
}