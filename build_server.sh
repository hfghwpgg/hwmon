#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER="${ROOT}/server"
BUILD="${SERVER}/build"

usage() {
  cat <<EOF
Usage: $(basename "$0") [help|debug|release|test|clear|iwyu]

Build the Hwmon server (CMake project in server/).

Subcommands:
  help     Show this message (default)
  debug    Configure and build with CMAKE_BUILD_TYPE=Debug
  release  Configure and build with CMAKE_BUILD_TYPE=Release
  test     Configure and build with unit tests enabled, then run them
  iwyu     Run include-what-you-use in debug mode and fix includes
           Build from iwyu won't be saved.
  clear    Remove server/build

Output binary: server/build/Hwmon

Prerequisites: CMake 3.30+, C++23-capable compiler (g++ or clang++).
Tests additionally require GoogleTest (GTest).
EOF
}

ensure_build_dir() {
  [ -d "$BUILD" ] || mkdir -p "$BUILD"
}

build_with_type() {
  local build_type="$1"
  local is_iwyu="${2:-false}"
  local -a cmake_extra=()
  if [[ "$is_iwyu" == true ]]; then
    cmake_extra+=(-DCMAKE_CXX_INCLUDE_WHAT_YOU_USE=include-what-you-use)
    rm -rf "$BUILD"
  fi
  ensure_build_dir
  cmake -S "$SERVER" -B "$BUILD" -DCMAKE_BUILD_TYPE="$build_type" "${cmake_extra[@]}"
  if [[ "$is_iwyu" == true ]]; then
    cmake --build "$BUILD" --parallel "$(nproc 2>/dev/null || echo 1)" > iwyu.log 2>&1
    iwyu-fix-includes < iwyu.log
    rm -rf "$BUILD"
  else
    cmake --build "$BUILD" --parallel "$(nproc 2>/dev/null || echo 1)"
  fi
}

run_tests() {
  ensure_build_dir
  cmake -S "$SERVER" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug -DHWMON_BUILD_TESTS=ON
  cmake --build "$BUILD" --parallel "$(nproc 2>/dev/null || echo 1)"
  ctest --test-dir "$BUILD" --output-on-failure
}

cmd="${1:-help}"

case "$cmd" in
  help)
    usage
    ;;
  debug)
    build_with_type Debug
    ;;
  release)
    build_with_type Release
    ;;
  test)
    run_tests
    ;;
  clear)
    rm -rf "$BUILD"
    ;;
  iwyu)
    build_with_type Debug true
    ;;
  *)
    echo "Unknown command: $cmd" >&2
    echo >&2
    usage >&2
    exit 1
    ;;
esac
