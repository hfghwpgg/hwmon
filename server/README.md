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
see [miniclient.py](miniclient.py)

## Server compilation

Recommended way of compiling server is using provided `build_server.sh` shell script
running it without any arguments will display help message

1. clone the repository

```bash
git clone https://github.com/hfghwpgg/hwmon.git
```

2. navigate to the project directory

```bash
cd hwmon
```

3. run provided shell script to compile the project

```bash
./build_server.sh debug
```

4. the compiled binary will be located in the `build` directory

```bash
./server/build/hwmon
```
