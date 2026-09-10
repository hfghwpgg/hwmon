# hwmon client

## Prerequisites

- Linux (tested on Arch Linux)
- g++ or clang++ with c++23 support (tested with g++ version 16.1.0)
  - Libraries:
    - `Qt6` with 'Widgets', 'Network' components
- CMake (tested with version 4.3.2)
- Make (tested with version 4.4.1)

## Client compilation

Recommended way of compiling client is using provided `build_client.sh` shell script

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
./build_client.sh debug
```

4. the compiled binary will be located in the `build` directory

```bash
./client/build/hwmonclient
```
