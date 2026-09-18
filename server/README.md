# hwmon server

## Prerequisites

- Linux (tested on Arch Linux)
- g++ or clang++ with c++23 support (tested with g++ version 16.1.0)
  - Libraries:
    - `nlohmann/json` library
    - `spdlog` library
    - `p-ranav/argparse` library 
    - `GTest` testing framework
    - `include-what-you-use` tool (optional)
- CMake (tested with version 4.3.2)
- Make (tested with version 4.4.1)

## How to query data?
see [miniclient.py](../miniclient.py)

<details>
  <summary><h2>Server compilation</h2></summary>

Recommended way of compiling server is using provided `build_server.sh` shell script  
running it without any arguments will display help message

1. Install dependencies

<details>
  <summary>Arch based</summary>

    sudo pacman -S --needed nlohmann-json spdlog gtest argparse make cmake gcc

</details>

<details>
  <summary>Debian based (Debian, Ubuntu, etc.)</summary>

    sudo apt install nlohmann-json3-dev spdlog-dev libgtest-dev \
      libargparse-dev cmake make gcc

</details>

<details>
  <summary>RHEL based (Red Hat, CentOS, etc.)</summary>

    sudo dnf install nlohmann-json-devel spdlog-devel gtest-devel cmake \
      make gcc gcc-c++ argparse-devel

</details>


  2. clone the repository
  ```bash
  git clone https://github.com/hfghwpgg/hwmon.git
  ```
  
  3. navigate to the project directory
  ```bash
  cd hwmon
  ```
  
  4. run provided shell script to compile the project
  ```bash
  ./build_server.sh debug
  ```
  
  5. the compiled binary will be located in the `build` directory
  ```bash
  ./server/build/hwmon
  ```
  
</details>
