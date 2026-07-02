# KNS — Kinetic Network Simulator

KNS (Kinetic Network Simulator) is an event-driven network simulator with a graphical interface. It models packet transmission, routing, TCP sessions, and link-level behavior in a way that is easy to inspect visually and extend over time.

The project is designed for experimentation, learning, and future research in networking topics such as retransmissions, sliding windows, congestion control, and custom link behaviors.

## Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Getting Started](#getting-started)
- [Build Requirements](#build-requirements)
- [Building](#building)
- [Running](#running)
- [Topology Files](#topology-files)
- [Project Structure](#project-structure)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)
- [Author](#author)

## Overview

KNS combines a discrete-event simulation core with a visual front end. Packets move through the simulated network according to routing decisions, link characteristics, and TCP session logic, while the GUI renders those packets in sync with simulation time.

The current version already includes:

- event-driven simulation
- synchronized packet animation
- TCP handshake and session tracking
- routing over arbitrary topologies
- multiple link modes
- packet delivery and latency tracking
- session-based traffic generation
- debugging and validation output

## Features

- Discrete-event simulation engine
- Visual packet animation synchronized with simulation time
- TCP handshake and session management
- Packet routing across network topologies
- Link modes:
  - full-duplex
  - half-duplex
  - simplex
- Packet delivery and latency statistics
- Session-level simulation statistics
- GUI for inspecting network activity in real time
- Validation output for simulation checks

## Architecture

KNS is organized into a few main layers:

### Simulation Core
Responsible for time management, event scheduling, statistics, and global simulation state.

### Network Layer
Handles topologies, links, routing tables, and packet forwarding.

### Transport Layer
Implements TCP connection state, handshake logic, and future reliability mechanisms.

### GUI Layer
Displays nodes, links, and animated packets in real time.

## Getting Started

Clone the repository, build the project, and run KNS with a topology file:

```bash
cmake -S . -B build
cmake --build build
./KNS [relative_path_to_topology]
```

## Build Requirements

- C++20
- CMake
- A compatible compiler toolchain
- GLFW
- ImGui
- nlohmann/json

## Building

```bash
cmake -S . -B build
cmake --build build
```

If you are using a multi-configuration generator, build the selected configuration accordingly.

## Running

```bash
./KNS <topology.json>
```

Example:

```bash
./KNS topologies/mesh4.json
```

## Topology Files

Topology files are JSON documents that define nodes and links used by the simulation.

A topology typically includes:

- node count
- link endpoints
- bandwidth
- propagation delay
- packet loss probability
- link mode

## Project Structure

```text
KNS/
├── app/
│   └── main.cpp
├── core/
│   ├── include/
│   └── src/
├── topologies/
│   └── mesh4.json
└── README.md
```

This layout may evolve as the TCP architecture becomes more modular.

## Roadmap

Planned future work includes:

- TCP retransmission timers
- Sliding window support
- ACK and sequence tracking refinement
- Congestion control
- Buffer management
- TCP connection termination
- Additional link types and behaviors
- Expanded simulation statistics
- More advanced packet visualizations

## Contributing

Contributions are welcome. Useful areas for future development include:

- TCP protocol behavior
- network modeling
- GUI improvements
- simulation accuracy
- topology and link handling
- test coverage

If you contribute, please keep the codebase consistent with the current event-driven architecture.

## License

Add your license here.

## Author

Kilop
