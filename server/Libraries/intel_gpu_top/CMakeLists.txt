add_library(igt OBJECT
  igt_perf.c
  intel_device_info.c
  intel_gpu_top.c
  intel_name_lookup_shim.c
)

target_include_directories(igt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# upstream code, not ours to lint
target_compile_options(igt PRIVATE -w)
