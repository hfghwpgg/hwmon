#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER="${ROOT}/server"
BUILD="${SERVER}/build"

usage() {
  cat <<EOF
Usage: $(basename "$0") [help|debug|release|clear]

Build the Hwmon server (CMake project in server/).

Subcommands:
  help     Show this message (default)
  debug    Configure and build with CMAKE_BUILD_TYPE=Debug
  release  Configure and build with CMAKE_BUILD_TYPE=Release
  clear    Remove server/build

Output binary: server/build/Hwmon

Prerequisites: CMake 3.30+, C++26-capable compiler (g++ or clang++).
EOF
}

ensure_build_dir() {
  [ -d "$BUILD" ] || mkdir -p "$BUILD"
}

build_with_type() {
  local build_type="$1"
  ensure_build_dir
  cmake -S "$SERVER" -B "$BUILD" -DCMAKE_BUILD_TYPE="$build_type"
  cmake --build "$BUILD" --parallel "$(nproc 2>/dev/null || echo 1)"
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
  clear)
    rm -rf "$BUILD"
    ;;
  *)
    echo "Unknown command: $cmd" >&2
    echo >&2
    usage >&2
    exit 1
    ;;
esac
