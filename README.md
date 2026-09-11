# KNS — Kinetic Network Simulator

KNS is a deterministic, discrete-event network simulator written in C++20. It
combines a reusable simulation library, a Dear ImGui desktop interface, and a
headless command-line runner for repeatable experiments.

The simulator models graph-based routing, bandwidth and propagation delay,
duplex link queues, packet loss, dynamic topology changes, and a focused TCP
subset with passive listeners, retransmission, delayed ACK, receive reordering,
and congestion-control state.

## Highlights

- deterministic event ordering by simulation timestamp and event ID;
- Dijkstra routing with delay, bandwidth, hop-count, and combined metrics;
- full-duplex, half-duplex, and simplex links with bounded queues;
- runtime link availability, delay, bandwidth, and topology changes;
- TCP handshake, listeners by node/port, backlog, RST rejection, and close;
- send/receive buffers, cumulative and delayed ACKs, RTT/RTO, Karn's rule,
  timeout retransmission, and fast retransmit;
- Tahoe, Reno, NewReno, and CUBIC congestion-control implementations;
- interactive packet visualization and congestion/latency panels;
- headless CSV export and Catch2 unit/integration tests.

KNS is a simulation model rather than a production TCP/IP stack. See
[TCP design](docs/tcp_design.md#current-limitations) for the exact boundary.

## Requirements

- CMake 3.20 or newer;
- a C++20 compiler;
- OpenGL development libraries;
- Git and network access during the first configure, because dependencies are
  resolved with CMake `FetchContent`.

CMake fetches nlohmann/json, Catch2, GLFW, Dear ImGui, and ImGuiFileDialog.

On Debian or Ubuntu, the graphical build commonly needs:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git \
  libgl1-mesa-dev xorg-dev \
  libwayland-dev wayland-protocols \
  libxkbcommon-dev extra-cmake-modules
```

## Build

Configure and build the application and tests:

```bash
cmake -S . -B build
cmake --build build
```

Useful configuration options:

| Option | Default | Purpose |
| --- | --- | --- |
| `KNS_BUILD_APP` | `ON` | Build the GUI/headless executable. |
| `KNS_BUILD_TESTS` | `ON` | Build and register the test suite. |
| `KNS_ENABLE_WARNINGS` | `ON` | Enable the project's compiler warnings. |

For a multi-config generator such as Visual Studio, select the configuration
explicitly when needed:

```powershell
cmake --build build --config Debug
```

## Run the GUI

The GUI accepts an optional topology file:

```powershell
# Windows with the Visual Studio generator
.\build\app\Debug\KNS.exe app\topologies\mesh4.json
```

```bash
# Linux/macOS with a single-config generator
./build/app/KNS app/topologies/mesh4.json
```

Without a positional topology, the application starts with an empty topology
and can load a file from the interface.

## Run headless

Headless mode requires a topology and writes aggregate statistics to CSV:

```powershell
.\build\app\Debug\KNS.exe `
  --headless `
  --topology app\topologies\mesh4.json `
  --routing-metric delay `
  --output results\mesh4-delay.csv
```

```bash
./build/app/KNS \
  --headless \
  --topology app/topologies/mesh4.json \
  --routing-metric delay \
  --output results/mesh4-delay.csv
```

### CLI options

```text
KNS [topology.json]
KNS --headless --topology <file> [--output <csv>]
    [--routing-metric <metric>]
```

| Option | Description |
| --- | --- |
| `--headless` | Run without creating the graphical interface. |
| `--topology <file>` | Load the specified topology JSON. Required headlessly. |
| `--output <csv>` | Set the output file; defaults to `results/results.csv`. |
| `--routing-metric <metric>` | Select the routing metric before traffic is scheduled. |
| `-h`, `--help` | Print usage and exit. |

Accepted routing metrics:

| Value | Route selection |
| --- | --- |
| `delay` | Lowest total propagation delay (default). |
| `bandwidth` | Highest bottleneck bandwidth. |
| `hop-count` | Fewest links. |
| `delay-bandwidth` | Lowest sum of link delay divided by bandwidth. |

An invalid metric is reported before the topology is loaded or the simulation
starts. `KNS_AUTO_START=0` disables automatic workload generation; note that a
headless run without traffic does not pass the engine's normal traffic
validation and therefore exits non-zero.

The exported CSV currently contains:

```text
packets_sent,packets_delivered,packets_lost,total_latency,
avg_latency,packets_in_transit,total_sessions
```

## Topology files

JSON topologies live in [`app/topologies/`](app/topologies/). A minimal example:

```json
{
  "nodes": 3,
  "name": "triangle",
  "links": [
    {
      "from": 0,
      "to": 1,
      "delay": 5.0,
      "bandwidth": 100.0,
      "loss": 0.01,
      "mode": "full_duplex"
    },
    {
      "from": 1,
      "to": 2,
      "delay": 10.0,
      "bandwidth": 50.0,
      "loss": 0.0,
      "mode": "half_duplex"
    }
  ]
}
```

Fields use milliseconds for `delay`, megabits per second for `bandwidth`, and a
probability from 0 to 1 for `loss`. Supported modes are `full_duplex`,
`half_duplex`, and `simplex`. Node IDs are zero-based. Self-loops, invalid
numeric values, and links to removed nodes are rejected.

Included examples:

- [`mesh4.json`](app/topologies/mesh4.json);
- [`mesh5.json`](app/topologies/mesh5.json);
- [`star.json`](app/topologies/star.json).

## Test

Run all registered tests after building:

```bash
ctest --test-dir build --output-on-failure
```

The suite covers:

- `tests/core/`: event ordering and engine lifecycle;
- `tests/network/`: links, queues, topology mutation, loading, and routing;
- `tests/tcp/`: state transitions, listeners, buffering, reliability, close,
  timers, and congestion control;
- `tests/integration/`: end-to-end simulation behavior;
- application CTest entries: accepted and rejected headless routing metrics.

## Experiments

[`scripts/run.py`](scripts/run.py) is the historical batch runner. It can launch
headless simulations, but its report parser currently expects an older CSV
schema than `SimulationEngine::exportStatsCSV()` emits. Use direct headless
commands for authoritative runs until those schemas are aligned. Measurement
definitions and earlier observations are documented in
[`docs/experiments.md`](docs/experiments.md).

## Architecture

```text
KNS/
├── app/                  GUI, CLI entry point, and bundled topologies
│   ├── gui/              Dear ImGui panels and rendering
│   └── main.cpp          Application setup and headless execution
├── core/                 Reusable simulation library
│   ├── include/          Public headers
│   └── src/              Implementations
├── docs/                 Maintainer-facing design documentation
├── scripts/              Experiment automation
├── tests/                Catch2 and integration tests
└── CMakeLists.txt        Root build configuration
```

The core has no dependency on the GUI. `SimulationEngine` owns the clock, event
queue, topology snapshot, routing tables, sessions, listeners, statistics, and
in-flight packet records. See [architecture](docs/architecture.md) and
[event engine design](docs/event_engine_design.md) for the governing invariants.

## Documentation

- [Architecture and design decisions](docs/architecture.md)
- [Event engine design](docs/event_engine_design.md)
- [TCP design](docs/tcp_design.md)
- [TCP protocol specification](docs/protocol_spec.md)
- [Experiments and measurement notes](docs/experiments.md)

## Current development areas

The main remaining integration and extension areas are:

- feed the advertised peer receive window back into normal sender flow control;
- gate packet generation with the selected congestion controller's `cwnd`;
- expand simultaneous-open/close and general RST behavior;
- add TCP options such as SACK and timestamps;
- implement dynamic routing protocols rather than centralized table rebuilds;
- add active queue-management policies such as RED;
- align the Python experiment runner with the current CSV schema.

## Contributing

1. Fork the repository and create a focused branch.
2. Keep core code independent from GUI dependencies.
3. Add or update tests for observable behavior.
4. Run `cmake --build build` and
   `ctest --test-dir build --output-on-failure`.
5. Use an English [Conventional Commit](https://www.conventionalcommits.org/)
   message and open a pull request explaining behavior and validation.

## License

KNS is source-available for personal, educational, research, and private
modification under the terms in [`LICENSE`](LICENSE). Commercial use is
prohibited unless separately authorized. This is not the MIT License.
