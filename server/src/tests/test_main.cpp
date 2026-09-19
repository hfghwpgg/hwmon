#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

// Custom entry point so we can silence the library's logging. The production
// code logs warnings/criticals on the error paths we deliberately exercise
// (e.g. unreadable sensors), which would otherwise spam the test output.
int main(int argc, char **argv) {
  spdlog::set_level(spdlog::level::off);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
