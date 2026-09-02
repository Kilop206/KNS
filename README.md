# KNS — Kinetic Network Simulator

KNS (Kinetic Network Simulator) is an event-driven deterministic network simulator with both an interactive graphical interface and a headless CLI execution mode. It models packet transmission, routing, TCP sessions, and link-level behavior in a way that is easy to inspect visually, experiment with, and extend over time.

The project is designed for experimentation, learning, and research in networking topics such as retransmissions, sliding windows, congestion control, dynamic routing, and custom link behaviors.

---

## Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Build Requirements](#build-requirements)
- [Building](#building)
- [Running](#running)
  - [GUI Mode](#gui-mode)
  - [Headless Mode](#headless-mode)
- [Automated Experiments & Benchmarking](#automated-experiments--benchmarking)
- [Running Tests](#running-tests)
- [Topology Files](#topology-files)
- [Project Structure](#project-structure)
- [Documentation](#documentation)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)
- [Author](#author)

---

## Overview

KNS combines a discrete-event simulation core with an interactive visual front-end and a headless CLI runner for batch experiments. Packets move through the simulated network according to routing decisions, link characteristics, and TCP session logic, while the GUI renders those packets in sync with simulation time.

The current version includes:
- **Deterministic Event-Driven Simulation**: Strict time ordering with incremental ID tie-breaking for 100% reproducible runs.
- **TCP Protocol Simulation**: Three-way handshake (SYN, SYN-ACK, ACK), data transfer, connection termination (FIN, FIN-ACK), timeout handling, and SYN retransmission counters.
- **Arbitrary Topologies & Routing**: Dijkstra-based shortest-path routing calculated over custom topologies with physical link constraints (bandwidth, latency, loss probability, duplex modes).
- **Interactive GUI**: Real-time packet animations with color coding by packet type, node inspection, latency graphs, and event logs.
- **Headless Mode & Python Automation**: Batch execution and parameter sweep scripts that output CSV reports and generate Matplotlib graphs.

---

## Features

- **Discrete-Event Simulation Core**: Millisecond-accurate logical ticks with strict FIFO tie-breaking for concurrent events.
- **Visual Packet Animation**: Real-time rendering synchronized with simulation speed, supporting interactive TCP connection initiation by dragging between nodes.
- **TCP Transport Layer**: Connection states (`CLOSED`, `LISTEN`, `SYN_SENT`, `SYN_RECEIVED`, `ESTABLISHED`, `CLOSE_WAIT`, `LAST_ACK`, `FIN_WAIT_1`, `FIN_WAIT_2`, `TIME_WAIT`), handshake logic, timeouts, and session management.
- **Physical Link Modeling**:
  - `full-duplex`
  - `half-duplex`
  - `simplex`
  - Link queue capacity limits and configurable loss rates.
- **Comprehensive Metrics & Stats**: Average latency, delivery rate, packet loss, throughput, and CSV export.
- **Headless Execution**: Fast simulation without graphical overhead for benchmarks and CI pipelines.
- **Extensive Test Suite**: Unit and integration tests powered by Catch2.

---

## Architecture

KNS is organized into distinct, decoupled layers:

### Simulation Core (`core/include/engine/core/` and `events/`)
Responsible for logical time progression (`SimulationClock`), priority event scheduling (`EventQueue`), global simulation state (`SimulationEngine`), metrics collection (`Stats`), and simulation events (`PacketGenerationEvent`, `PacketReceivedEvent`, `TCPHandshakeEvent`, etc.).

### Network Layer (`core/include/network/`)
Handles graph topologies (`Topology`), link constraints and duplex modes (`Link`), routing tables via Dijkstra (`Routing`), and JSON topology parsing (`TopologyLoader`).

### Transport Layer (`core/include/network/transport/tcp/`)
Implements TCP endpoints (`TCPConnection`), session tracking (`TCPSession`), state machine transitions (`TCPStateMachine`), and TCP segment encapsulation (`TCPSegment`).

### GUI Layer (`app/` and `app/gui/`)
Interactive Dear ImGui / GLFW / OpenGL interface providing live topology visualization, packet rendering (`PacketRenderer`), latency charts, node routing details, and event log inspection.

---

## Build Requirements

- **C++20** compatible compiler (GCC 11+, Clang 13+, or MSVC 2019+)
- **CMake 3.20+**
- **OpenGL** development libraries

All other third-party dependencies (**GLFW**, **Dear ImGui**, **ImGuiFileDialog**, **nlohmann/json**, and **Catch2**) are downloaded and built automatically via CMake `FetchContent`.

### Linux (Debian / Ubuntu) Dependencies

```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake \
    libgl1-mesa-dev \
    xorg-dev \
    libwayland-dev \
    wayland-protocols \
    libxkbcommon-dev \
    extra-cmake-modules
```

---

## Building

```bash
# Configure the project
cmake -S . -B build

# Build all targets (simulator and tests)
cmake --build build
```

---

## Running

### GUI Mode

Launch KNS with an optional topology JSON file (or pick one inside the GUI file dialog):

```bash
# Windows
.\build\app\KNS.exe app/topologies/mesh4.json

# Linux / macOS
./build/app/KNS app/topologies/mesh4.json
```

### Headless Mode

Run simulations directly from the command line without opening the GUI window, exporting results to a CSV file:

```bash
# Windows
.\build\app\KNS.exe --headless --topology app/topologies/mesh4.json --output results.csv

# Linux / macOS
./build/app/KNS --headless --topology app/topologies/mesh4.json --output results.csv
```

---

## Automated Experiments & Benchmarking

The project includes an automation runner script located in `scripts/run.py` (v1.2). It executes headless simulations, performs parameter sweeps (varying packet loss, packet size, bandwidth), and generates Matplotlib graphs and summary reports.

```bash
# Run automated benchmark suite
python scripts/run.py
```

Generated plots and summary statistics are saved under the `results/` directory.

---

## Running Tests

KNS has a comprehensive suite of unit and integration tests written in **Catch2**:

```bash
# Run tests using CTest
ctest --test-dir build --output-on-failure

# Or run the test executable directly
# Windows:
.\build\tests\kns_tests.exe

# Linux / macOS:
./build/tests/kns_tests
```

Test coverage includes:
- `tests/core/`: Event queue ordering, FIFO tie-breaking, and simulation clock behavior.
- `tests/network/`: Link duplex modes, transmission delays, queue capacities, and Dijkstra routing tables.
- `tests/tcp/`: State machine transitions, handshake, active/passive close, SYN retry limits, and determinism.
- `tests/integration/`: End-to-end full simulation validation.

---

## Topology Files

Topologies are declared as JSON files under `app/topologies/`:

```json
{
  "nodes": 4,
  "name": "mesh4",
  "links": [
    {
      "from": 0,
      "to": 1,
      "delay": 5,
      "bandwidth": 100,
      "loss": 0.0,
      "mode": "full_duplex"
    },
    {
      "from": 1,
      "to": 2,
      "delay": 10,
      "bandwidth": 100,
      "loss": 0.0,
      "mode": "full_duplex"
    }
  ]
}
```

Pre-configured topologies include:
- `app/topologies/mesh4.json` (4-node mesh)
- `app/topologies/mesh5.json` (5-node mesh)
- `app/topologies/star.json` (Star topology)

---

## Project Structure

```text
KNS/
├── app/                      # Application entry point and GUI
│   ├── gui/                  # ImGui panels, packet rendering, and themes
│   ├── topologies/           # Pre-defined topology JSON files
│   └── main.cpp              # CLI parser, simulation loop, and GUI initialization
├── core/                     # Core simulation engine (no GUI dependencies)
│   ├── include/
│   │   ├── engine/           # Event queue, clock, stats, and simulation events
│   │   ├── enums/            # PacketType, TCPState, LinkMode enums
│   │   └── network/          # Topology, Link, Routing, and TCP transport
│   └── src/                  # Implementation files
├── docs/                     # Architectural and design documentation
│   ├── architecture.md       # High-level design decisions
│   ├── protocol_spec.md      # TCP subset protocol specification
│   ├── tcp_design.md         # TCP connection/session architecture
│   └── experiments.md        # Research and experiment notes
├── results/                  # Generated benchmark summaries and graphs
├── scripts/                  # Python runner and benchmark scripts (run.py)
├── tests/                    # Catch2 unit and integration tests
│   ├── core/                 # Event engine tests
│   ├── network/              # Link and routing tests
│   ├── tcp/                  # TCP state machine, handshake, and determinism tests
│   └── integration/          # End-to-end simulation tests
├── CMakeLists.txt            # Root build configuration
└── README.md
```

---

## Documentation

Detailed technical documentation is available in the [`docs/`](docs/) directory:
- [Architecture & Design Decisions](docs/architecture.md)
- [TCP Protocol Specification](docs/protocol_spec.md)
- [TCP Architecture Design](docs/tcp_design.md)
- [Experimentation Reports](docs/experiments.md)

---

## Roadmap

Planned future work includes:
- [ ] Dynamic RTT estimation & adaptive Retransmission Timeout (RTO)
- [ ] Sliding Window & flow control support
- [ ] Congestion Control algorithms (Slow Start, Congestion Avoidance, Fast Retransmit / Fast Recovery)
- [ ] Dynamic routing protocols (e.g. RIP / OSPF-like link state updates on link failure)
- [ ] In-GUI Interactive Topology Editor (add/remove nodes and links visually)
- [ ] Buffer management policies (e.g. RED - Random Early Detection)

---

## Contributing

Contributions are welcome! If you would like to contribute:
1. Fork the repository.
2. Create a feature branch (`git checkout -b feature/amazing-feature`).
3. Ensure all tests pass (`ctest --test-dir build --output-on-failure`).
4. Commit your changes following standard conventional commits.
5. Open a Pull Request.

---

## License

This project is licensed under the [MIT License](LICENSE).

---

## Author

**Kilop / Guilherme Döge**
