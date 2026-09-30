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
  <summary><h2>API</h2></summary>

The server listens on the abstract Unix domain socket `@hwmon`. The address is a leading NUL byte followed by the name `hwmon`; Clients send one JSON object per line and read one JSON value per line. A line is a request only after the trailing `\n`.

A short client [miniclient.py](../miniclient.py) is mainly used for inspecting and testing.

### Commands

Every request is a JSON object with a string `cmd`.

| `cmd` | Extra fields | Success response |
| --- | --- | --- |
| `get` | none | the current snapshot (a JSON array) |
| `ping` | none | `{"ok": true}` |
| `reset` | none | `{"ok": true}` |
| `set_interval` | `value`: unsigned integer, milliseconds, minimum 50 | `{"ok": true, "interval": <value>}` |

`set_interval` changes how often the runner publishes a new snapshot. The default interval is 1000 ms (`--interval`).

`reset` asks the runner to clear aggregated readings. The ack means the flag was set. The snapshot already published is unchanged until the next sample.

Anything else returns an error object and keeps the connection open:

| Condition | Response |
| --- | --- |
| body is not JSON | `{"error": "invalid json"}` |
| JSON is not an object, or `cmd` is missing or not a string | `{"error": "missing cmd"}` |
| `get` before the first snapshot is published | `{"error": "no data yet"}` |
| `set_interval` without an unsigned `value` | `{"error": "set_interval requires unsigned 'value'"}` |
| `set_interval` with `value` &lt; 50 | `{"error": "interval must be >= 50"}` |
| unrecognized `cmd` | `{"error": "unknown command", "cmd": "<what was sent>"}` |

Two failures close the connection after a single error line:

| Condition | Response |
| --- | --- |
| another client would exceed `--max-clients` (default 5) | `{"error": "too many clients"}` |
| unread input grows past the request limit | `{"error": "request too large"}` |

### Request and response format

- Transport is `AF_UNIX` / `SOCK_STREAM`.
- Framing is newline-delimited UTF-8 JSON. There is no length prefix.
- One request line produces one response line. Several complete lines in one `send` are answered in order.
- Control responses are JSON objects. A successful `get` is a JSON array, not wrapped in `{"ok": ...}`.
- `get` returns the published string as-is, plus the framing newline.

`get` shape (example)

```json
[
  {"timestamp": 1190636934},
  {
    "name": "AMD Ryzen 5 5600X 6-Core Processor",
    "type": 0,
    "sensors": [
      {
        "name": "Tctl",
        "type": 0,
        "isPrimary": false,
        "readings": {
          "value": 37.0,
          "min_value": 37.0,
          "max_value": 38.125,
          "sum": 264.125,
          "times": 7
        }
      }
    ]
  }
]
```

The first element is `timestamp`: Unix time (seconds) when the current aggregation window opened (reading start, updating on `reset`)

Each following element is one device:

| Field | Meaning |
| --- | --- |
| `name` | device name |
| `type` | `DeviceType` (see below) |
| `sensors` | array of sensors. Omitted when the device has none |

Each sensor:

| Field | Meaning |
| --- | --- |
| `name` | sensor name |
| `type` | `SensorType` (see below) |
| `isPrimary` | boolean. The client treats primary sensors as the headline reading for that device |
| `readings.value` | latest successful sample |
| `readings.min_value` / `max_value` | extremes since the window opened |
| `readings.sum` | sum of samples since the window opened |
| `readings.times` | number of samples in `sum` |

`readings` stay at their previous values when a sample fails. A sensor that has never produced a value still appears, with non-finite `value` / `min_value` / `max_value` until the first good sample.

Device `type`:

| Value | Name |
| --- | --- |
| 0 | CPU |
| 1 | GPU |
| 2 | RAM |
| 3 | STORAGE |
| 4 | NETWORK |
| 5 | UNKNOWN (other sysfs hwmon devices) |

Sensor `type`, and the unit of `readings.value` after scaling:

| Value | Name | Unit |
| --- | --- | --- |
| 0 | TEMPERATURE | °C |
| 1 | FAN_SPEED | RPM |
| 2 | FREQUENCY | MHz |
| 3 | POWER | W |
| 4 | VOLTAGE | V |
| 5 | CURRENT | A |
| 6 | ENERGY | raw |
| 7 | UTILIZATION | percent |
| 8 | MEMORY | bytes |
| 9 | THROUGHPUT | bytes/s |
| 10 | UNKNOWN | unscaled |

### Request size limit

Each connection may buffer at most 64 KiB of unread input. The check runs after every `recv`, before the line is parsed.

If the buffer grows past that, the server writes `{"error": "request too large"}` and drops the connection. Complete lines already answered do not count: the limit is the pending buffer, not the total bytes sent over the life of the socket. A single request line has to fit in that 64 KiB, including any incomplete trailing fragment.

Responses are not capped. A `get` snapshot can be larger than 64 KiB; the server writes it in full.

### Access model

The socket lives in the Linux abstract namespace. It has no path, owner, or mode bits, and nothing is left on disk when the server exits.

There is no authentication token and no peer-credential check. Any local process that can open a Unix socket can connect and call every command, including `set_interval` and `reset`.

- The address is `@hwmon` (a leading NUL byte followed by `hwmon`). Clients use that name; there is no path option.
- A second server cannot bind the same name while the first is listening (`EADDRINUSE`). The name is released as soon as the listening socket is closed, including when the process exits or crashes.
- Privileges are still dropped after startup (`setuid`/`setgid` to `SUDO_UID` / `SUDO_GID` when started through `sudo`). That stops the process from staying root. It does not limit who can connect. `--dont-drop-root` skips the drop.
- At most `--max-clients` connections are served at once (default 5, maximum 1024). Further connections receive `{"error": "too many clients"}` and are closed. The listen backlog (`--backlog`, default 6) only queues connections waiting to be accepted.

### Snapshot guarantees

- The runner samples every device, serializes them into one JSON string, then publishes that string with an atomic store. `get` loads the pointer and returns that string. A client never observes a half-written document.
- The published string is immutable. A snapshot already returned stays valid and unchanged after a newer one is published.
- One `get` is exactly one generation. The next `get` may be a newer generation. There is no sequence number.
- Devices in one snapshot were all read in that same runner iteration, then serialized together. Reads inside the iteration are sequential, not one simultaneous sample.
- `timestamp` and the `min_value` / `max_value` / `sum` / `times` fields share one window. The window opens at startup and again on the first sample after `reset`. `value` is only the latest sample inside that window.
- `{"ok": true}` from `reset` does not mean aggregates are already cleared. Clearing happens on the following iteration, and the next `get` is the first snapshot of the new window.
- Before the runner publishes anything, `get` returns `{"error": "no data yet"}`.

</details>

<details>
  <summary> <h2>Architecture</h2> </summary>

### Overview
At first, main has 2 threads: one for the runner and one for the UDS server.
UDS server creates a thread for each client connection, up to a configurable limit.
If we are running as root, we let runner initialize all devices, and then we drop them
before setting up UDS server

### Runner
Runner is responsible for creating devices and reading their values, as well as publishing a snapshot of them to the UDS server through SharedState.

### Device
Device acts as a container for sensors of each peripheral. It is responsible for initializing and reading sensors, as well as sending reqests down to them (such as `reset`).
Depending on the sensor type, it may also be responsible for pushing value to a sensor (see [AmdGpuDevice.cpp](./src/Devices/GPU/AmdGpuDevice.cpp)), or preparing it altogher (see utilization reading in [CpuDevice.cpp](./src/Devices/CpuDevice.cpp))
Device itself is an interface.
Each device takes at least `name`: (string) and `type`: (DeviceType) parameters

### Sensor
Sensor is responsible for reading a single sensor value from a device.
It shouldn't be initialized directly, but rather through functions `makeFileSensor` or `addPushSensor`.  
 - `makeFileSensor` has `void` return type. Requires:
   - `Transform` (see below)
   - `sensors` (vector of unique pointers to sensors)
   - `path` (file path)
   - `SensorConfig` (sensor configuration)  
 - `addPushSensor` returns `PushSource*`, which is used by device for pushing values to the sensor. Requires:
   - `Transform` (see below)
   - `sensors` (vector of unique pointers to sensors)
   - `SensorConfig` (sensor configuration)  

Transform type: 
  - Scale: scales the sensor value by a given factor
  - Delta: calculates the difference between consecutive sensor readings and divides by time elapsed
    > (`(currVal - lastVal) / (timeDelta * divider))
Source type: 
  - File: reads sensor value from a file. 
  - Push: Device pushes value to the sensor

#### SensorConfig fields
- name: (string) sensor name
- type: (SensorType) sensor type
- divider: (int) divider for the sensor value (0 means automatic, deduced from sensor type)
- aggregateData: (bool) whether to aggregate data from multiple readings (optional, default true)
- isPrimary: (bool) whether the sensor is a primary sensor (optional, default false)

### SensorReading
SensorReading is a struct that holds sensor reading data.
Fields: value, min_value, max_value, sum, times.
 > average is calculated from sum / times
reset() sets the three values to NaN, sum to 0, and times to 0. 
NaN is used when we dont have a valid value to return, but we expect that (such as DeltaTransform readings)
SourceStatus is return when value is invalid and we didn't expect it to be such.

</details>

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
  ./server/build/hwmon-server
  ```
  
</details>
