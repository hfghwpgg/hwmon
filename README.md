# Hwmon

Tool for monitoring temperature, voltages and more in real time.

> currently WIP

## Prerequisites

- Linux (tested on Arch Linux)
- g++ or clang++ (tested with g++ version 16.1.0)
- CMake (tested with version 4.3.2)
- Make (tested with version 4.4.1)

## Compilation steps (for server)

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
./hwmon
```

TODO:

- finish server
- make client
