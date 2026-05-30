#include "Runner.hpp"
#include <memory>
#include <print>

int main() {
  auto runner = std::make_unique<Runner>();
  runner->Run();

  std::print("program finished\n");
  return 0;
}