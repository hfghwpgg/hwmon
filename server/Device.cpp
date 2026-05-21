#include <filesystem>
#include <iostream>
#include <map>
#include <ostream>
#include "Device.hpp"
#include "SensorWhitelist.hpp"

namespace fs = std::filesystem;
using std::string;
using std::vector;

Device::Device(fs::path path) : path(path), Sensors() {
    Initialize();
}

Device::~Device() {
    std::cout << "Device destroyed" << std::endl;
}

void Device::Initialize() {
    std::map<string, vector<string>> available_sensors;
    for (const auto &entry : fs::directory_iterator(path))
    {
        if (entry.is_regular_file())
        {
            // stem returns filename
            // without extension
            string filename = entry.path().stem();
            size_t underscorePos = filename.find('_');
            if (underscorePos != string::npos)
            {
                string part1 = filename.substr(0, underscorePos);
                string part2 = filename.substr(underscorePos + 1);

                if (!IsWhitelistedSensorAttribute(part2))
                    continue;

                if (available_sensors.count(part1))
                {
                    available_sensors.at(part1).push_back(part2);
                }
                else
                {
                    available_sensors.insert({part1, vector<string>{part2}});
                }
            } else {
                // workaround for pwm sensor, which exposes their value in ex. 'pwm1' file
                available_sensors.insert({filename, vector<string>{filename}});
            }
        }
    }
}

void Device::Read() {
    for (auto a : available_sensors) {
        std::cout << a.first << std::endl;
        for (string v : a.second)
        {
            std::cout << v << ',';
        }
        std::cout << std::endl;
    }
}