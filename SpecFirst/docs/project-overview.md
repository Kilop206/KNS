# KNS — Project Overview

## 1. Project Identity

**KNS (Kinetic Network Simulator)** is a C++20 discrete-event network simulator designed to model and visualize network behavior through deterministic simulation.

The project combines:

* a network simulation core;
* topology and link management;
* packet transmission;
* routing;
* event scheduling;
* a simplified TCP communication model;
* a graphical interface;
* headless execution;
* automated testing.

---

## 2. Primary Objective

The primary objective of KNS is to provide a controllable and observable environment in which network behavior can be modeled, simulated, inspected, and tested.

The simulator should make network behavior explicit rather than hiding it behind real-time execution.

---

## 3. Core Characteristics

### 3.1 Discrete-event simulation

KNS models network activity through events scheduled on a logical simulation timeline.

Simulation time is independent of wall-clock time.

This allows the same initial conditions to produce reproducible execution.

### 3.2 Deterministic behavior

Given equivalent:

* topology;
* configuration;
* initial state;
* event ordering;

the simulator should produce equivalent results.

### 3.3 Network topology

The topology model represents nodes and the links connecting them.

Links have operational state and transmission-related constraints.

Topology state directly affects routing and transmission.

### 3.4 Routing

KNS supports network path selection and next-hop decisions.

Routing must respect the current topology and operational state of links.

### 3.5 Packet transmission

Packets move through the simulated topology according to scheduled events and link behavior.

Transmission must account for:

* link availability;
* queue/capacity constraints;
* packet delivery;
* event timing.

### 3.6 Simplified TCP

KNS contains a simplified TCP model supporting connection establishment, data transfer, termination, and TIME_WAIT behavior.

The current model includes:

```text
SYN
SYN-ACK
ACK
DATA
ACK
FIN
TIME_WAIT
expiration
```

The TCP implementation is intentionally a simulation model rather than a production TCP stack.

---

## 4. Graphical Interface

KNS provides a GUI based on:

* Dear ImGui;
* GLFW;
* OpenGL.

The GUI allows users to inspect and interact with the simulated network.

GUI responsibilities include:

* rendering;
* node manipulation;
* configuration;
* simulation controls;
* status visualization;
* statistics.

The GUI must remain separate from the simulation core.

---

## 5. Simulation Lifecycle

The simulation lifecycle is explicitly represented by:

```text
READY
  ↓ Start
RUNNING
  ↓ Pause
PAUSED
  ↓ Resume
RUNNING
  ↓ event queue exhausted
FINISHED
```

### READY

The simulation is loaded and has not begun execution.

### RUNNING

Events are being processed.

### PAUSED

Events remain available but execution is temporarily stopped.

### FINISHED

The event queue has been exhausted after simulation execution.

The absence of events before execution must not by itself cause `FINISHED`.

---

## 6. Network Construction vs. Execution

KNS distinguishes between constructing a network scenario and executing it.

For example, connecting two nodes may create or schedule a TCP connection, but that operation must not implicitly start simulation execution.

The intended behavior is:

```text
User connects nodes
        ↓
TCP connection scheduled
        ↓
Simulation remains paused
        ↓
User starts simulation
        ↓
Events are processed
```

This distinction is essential for predictable GUI behavior.

---

## 7. Execution Modes

KNS supports both graphical and headless execution.

### GUI mode

Used for:

* visualization;
* interactive topology manipulation;
* simulation control;
* inspection.

### Headless mode

Used for:

* automated execution;
* testing;
* reproducible scenarios;
* environments without a graphical display.

Core behavior must remain independent of the execution mode.

---

## 8. Testing

KNS uses automated tests to verify core behavior.

Testing covers areas including:

* topology;
* links;
* routing;
* packets;
* simulation;
* TCP;
* loader validation;
* integration behavior.

Tests must focus on observable contracts and state transitions.

---

## 9. Build Environment

KNS is built as a C++20 project.

The intended Windows MinGW environment is:

```text
C:\mingw64
```

Build configuration must not mix the MinGW environment with MSYS2 UCRT64 runtime artifacts.

A clean toolchain configuration is required for reproducible binaries.

---

## 10. Project Development Model

Development follows a specification-first process.

The normal sequence is:

```text
Issue / requirement
       ↓
Current implementation analysis
       ↓
Specification
       ↓
Acceptance criteria
       ↓
Implementation
       ↓
Tests
       ↓
Review
       ↓
Documentation
```

Historical issues must be validated against the current implementation before work begins.

---

## 11. Current Engineering Priorities

Current development areas include:

* simulation lifecycle correctness;
* topology and link correctness;
* packet transmission semantics;
* routing behavior;
* TCP state handling;
* event scheduling;
* dynamic topology behavior;
* GUI/simulation separation;
* deterministic execution;
* test coverage.

Specific priorities are tracked through the project issue system and the implementation plan.
