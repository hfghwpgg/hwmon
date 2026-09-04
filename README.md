# Hwmon

Tool for monitoring temperature, voltages and more in real time.

> currently WIP

## Prerequisites

- Linux (tested on Arch Linux)
- g++ or clang++ with c++23 support (tested with g++ version 16.1.0)
  - Libraries:
    - `nlohmann/json` library
    - `spdlog` library
    - `include-what-you-use` tool (optional)
    - `GTest` testing framework (optional)
- CMake (tested with version 4.3.2)
- Make (tested with version 4.4.1)

## Server compilation

Recommended way of compiling server is using provided `build_server.sh` shell script

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
> running this script without any flags will display help message  
> static build will download and compile necessary libraries  
> [supports only debug/release, without tests]

4. the compiled binary will be located in the `build` directory

```bash
./server/build/Hwmon
```

### Manual compilation

1. clone the repository

```bash
git clone https://github.com/hfghwpgg/hwmon.git
```

2. navigate to the project directory

```bash
cd hwmon/server
```

3. create a build directory and navigate into it

```bash
mkdir build && cd build
```

4. run CMake to configure the project

```bash
cmake ..
```

5. compile the project using Make

```bash
make
```

6. the compiled binary will be located in the `build` directory

```bash
./Hwmon
```

Special thanks:
 - [btop](https://github.com/aristocratos/btop/) project
    - fully skided gpu reading

- [Hwmon-python](https://github.com/guicalare/Hwmon-python)
    - some sysfs paths

TODO:

- server
  - better error handling (dont know which error to abort on, and which just print out)
  - better device name deducing [kinda done?]
  - gpu support [done, needs more testing now]
  - ram usage monitoring
  - cmdline arguments
  - more testing, preferably on other pc's

- make client
