# KNS — Data Model

## 1. Purpose

This document defines the principal runtime entities and their relationships in KNS.

It describes the current data model and must remain aligned with the implementation.

---

## 2. Simulation

The simulation is driven by an event queue and logical simulation time.

Conceptually:

```text
Simulation
├── SimulationState
├── Logical Time
└── Event Queue
```

The simulation lifecycle is represented explicitly by:

```text
Ready
Running
Paused
Finished
```

An empty event queue does not, by itself, define the lifecycle state.

---

## 3. Topology

The topology represents the simulated network.

```text
Topology
├── Nodes
└── Links
```

Nodes represent network endpoints or intermediate network entities.

Links represent connectivity between nodes.

---

## 4. Node

A node is identified by an integer node identifier.

Nodes may participate in:

* routing;
* packet transmission;
* TCP connections;
* simulation events.

---

## 5. Link

A link connects two nodes.

A link has an operational state:

```text
UP
DOWN
```

A link also maintains transmission-related state, including its packet queue and queue capacity.

The presence of a queue does not by itself define complete FIFO semantics; FIFO behavior must follow the actual transmission contract and implementation.

---

## 6. Packet

A packet represents simulated network data transported through the topology.

Packets may contain protocol-specific information depending on the transport or network behavior being simulated.

Packet processing occurs through simulation events rather than direct real-world network transmission.

---

## 7. TCP Session

A `TCPSession` represents a simulated TCP communication between a source and destination node.

The current implementation contains:

```text
TCPSession
├── session_id
├── source
├── destination
├── state
├── total_packets
├── packets_sent
├── close_requested
├── traffic_generated
├── client_connection
└── server_connection
```

A session therefore contains two `TCPConnection` instances:

```text
TCPSession
├── client_connection
└── server_connection
```

---

## 8. TCP Connection

A `TCPConnection` represents one side of a simulated TCP connection.

It contains:

```text
TCPConnection
├── TCP State Machine
├── Sequence Number
├── Expected ACK Number
├── Local Node
├── Remote Node
└── SYN Retry State
```

The connection provides operations for:

* SYN;
* SYN-ACK;
* ACK;
* FIN;
* SYN retry handling;
* TIME_WAIT expiration;
* TCP state transitions.

---

## 9. TCP State

TCP state is represented using the project's `TCPState` model.

The exact state set is defined by the current TCP implementation and must not be duplicated independently in documentation.

---

## 10. Routing

Routing operates over the current topology.

A route consists conceptually of:

```text
Source
 ↓
Intermediate Nodes
 ↓
Destination
```

The current routing implementation excludes links that are not operational.

In particular:

```text
Link DOWN
    ↓
Routing
    ↓
Link is not considered available
```

Routing behavior must remain independent from GUI presentation.

---

## 11. Events

Simulation behavior is executed through scheduled events.

Events may represent operations such as:

* packet generation;
* packet transmission;
* packet reception;
* TCP handshake processing;
* TCP timeout processing;
* TCP termination;
* TIME_WAIT expiration.

The event queue is part of the simulation engine rather than the topology data model.

---

## 12. Relationship Overview

```text
SimulationEngine
        │
        ├── SimulationState
        ├── Event Queue
        │
        └── Topology
              │
              ├── Nodes
              │
              └── Links
                    │
                    └── Packet Transmission

TCP Session
        │
        ├── Client TCPConnection
        │
        └── Server TCPConnection
```

---

## 13. Documentation Status

The data model distinguishes between current implementation and future requirements.

### Implemented

Behavior directly represented by the current repository.

### Required

Behavior required by project specifications but not necessarily fully implemented.

### Planned

Behavior intentionally reserved for future implementation.

Documentation must not present `Required` or `Planned` behavior as implemented.
