#include "Device.hpp"
#include "EnergySensor.hpp"
#include "Runner.hpp"
#include "Sensor.hpp"
#include "SensorTypes.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <print>
#include <thread>

int main() {
  static int interval = 1000;
  auto runner = std::make_unique<Runner>(interval);
  runner->Run();

  // Sensor test{"/sys/class/hwmon/hwmon6/temp1_input", "testname", SensorType::TEMPERATURE};
  // EnergySensor test2{"/sys/class/hwmon/hwmon7/energy7_input", "testname2", SensorType::ENERGY};


  // Sensor *d[] = {&test, &test2};
  // for (auto &sen : d) {
  //   sen->updateValue();
  //   std::this_thread::sleep_for(std::chrono::seconds{1});
  //   sen->updateValue();
  //   const auto s = sen->serialize();
  //   std::string ss = s.dump();

  //   std::cout << ss << '\n';
  // }
  //


  std::print("program finished\n");
  return 0;
}
