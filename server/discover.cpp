#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <vector>
#include "discover.hpp"

void test()
{
    const std::string GPU_MON_PATH{"/sys/class/hwmon/hwmon3"};
    std::vector<std::string> v{
        GPU_MON_PATH + "/temp1_input",
        GPU_MON_PATH + "/temp2_input",
        GPU_MON_PATH + "/temp3_input",
    };

    std::vector<std::ifstream> infs;
    for (const auto &path : v)
    {
        infs.emplace_back(path);
    }

    std::string str;
    for (int i = 0; i < 5; i++)
    {
        for (auto &inf : infs)
        {
            std::getline(inf, str);
            std::cout << str << std::endl;
            inf.seekg(0);
        }
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }

    for (auto &inf : infs)
    {
        inf.close();
    }
}