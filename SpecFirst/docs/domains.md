# KNS — Domain Model

This document defines the principal domains of the KNS (Kinetic Network Simulator) and the responsibilities that belong to each domain.

The purpose is to keep domain responsibilities explicit and prevent unrelated concerns from leaking between components.

---

## 1. Domain Overview

KNS can be understood through the following primary domains:

```text
KNS
├── Simulation
├── Topology
├── Network
│   ├── Nodes
│   ├── Links
│   ├── Packets
│   └── Routing
├── TCP
├── Persistence / Loading
├── Application / GUI
└── Testing
```

These domains interact, but they should remain conceptually separated.

---

## 2. Simulation Domain

The Simulation domain controls discrete-event execution.

### Responsibilities

* Maintain logical simulation time.
* Maintain the event queue.
* Schedule events.
* Process events.
* Control simulation execution.
* Represent simulation lifecycle state.
* Support execution operations such as start, pause, resume, and step where provided by the implementation.

### Core principle

Simulation execution is independent of wall-clock time.

The simulation must not require the GUI to exist in order to execute core behavior.

### Lifecycle

The intended lifecycle is:

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

The domain must distinguish an unstarted simulation from a simulation that has completed all scheduled work.

---

## 3. Topology Domain

The Topology domain represents the network graph.

### Responsibilities

* Manage network nodes.
* Manage network links.
* Maintain connectivity relationships.
* Validate topology operations.
* Provide topology queries.
* Support topology mutation.

The topology is the authoritative representation of network connectivity.

### Invariants

Operations must preserve topology consistency.

Examples include:

* links must reference valid endpoints;
* node and link lookup must have defined failure behavior;
* invalid topology mutations must not silently succeed;
* removed entities must not remain reachable through normal topology APIs.

---

## 4. Node Domain

A node represents an endpoint in the simulated network.

Nodes participate in:

* topology relationships;
* routing;
* packet transmission;
* TCP connections.

Node identity must be stable within the scope required by the simulation.

APIs operating on node identifiers must define behavior for invalid or unknown identifiers.

---

## 5. Link Domain

A Link represents a network connection between nodes.

### Responsibilities

* Represent the relationship between two nodes.
* Represent operational state.
* Control transmission constraints.
* Participate in routing.
* Participate in packet delivery.
* Manage queue/capacity behavior where supported.

### Operational state

Links may be:

```text
UP
DOWN
```

A `DOWN` link must not be treated as available for normal routing or transmission.

### Queue behavior

Where a link has a transmission queue, queue semantics must be explicitly defined.

FIFO behavior should be preserved when FIFO is part of the link contract.

---

## 6. Packet Domain

Packets represent units of network data transported through the simulated topology.

### Responsibilities

* Represent packet data and metadata.
* Identify source and destination.
* Carry protocol-specific information where required.
* Participate in transmission events.
* Preserve sufficient transmission context for correct delivery.

Packets already in transit must not become ambiguously associated with another link because the topology changes.

---

## 7. Routing Domain

The Routing domain determines how traffic moves through the topology.

### Responsibilities

* Determine reachable paths.
* Determine next hops.
* Respect current link state.
* Handle unreachable destinations.
* Validate node identifiers and routing requests.

Routing must operate against the current topology state.

A route must not normally depend on a `DOWN` link.

### Next-hop contract

`getNextHop()`-style APIs must explicitly define:

* valid input nodes;
* destination behavior;
* invalid identifiers;
* unreachable destinations;
* bounds;
* failure representation.

---

## 8. TCP Domain

The TCP domain models simplified TCP communication.

It is intentionally not a complete implementation of the real TCP specification.

### Responsibilities

* Create logical TCP connections.
* Manage TCP connection state.
* Exchange control packets.
* Transfer data.
* Process acknowledgements.
* Perform connection termination.
* Handle TIME_WAIT behavior.

### Simplified lifecycle

```text
Connection creation
        ↓
SYN
        ↓
SYN-ACK
        ↓
ACK
        ↓
Established
        ↓
DATA / ACK
        ↓
FIN
        ↓
TIME_WAIT
        ↓
Expiration
```

The actual implementation remains authoritative for the exact set of states and transitions.

---

## 9. TCP Connection Domain

A TCP connection represents protocol state associated with communicating endpoints.

It should provide explicit behavior for:

* connection establishment;
* established communication;
* data transmission;
* acknowledgement;
* termination;
* timeout or expiration;
* invalid transitions.

State transitions must be deterministic.

Failures during transitions must not be silently converted into successful transitions.

---

## 10. TCP Session Domain

A TCP session may aggregate or coordinate connection-level behavior.

The relationship between session-level state and individual connection state must remain consistent.

If the implementation exposes aggregate session state, its contract must define:

* what state is represented;
* how multiple connections affect the aggregate;
* how failure is represented;
* how termination affects the aggregate.

The aggregate state must not contradict the underlying connection states.

---

## 11. Persistence and Topology Loading

The persistence/loading domain converts external topology representations into KNS runtime structures.

### Responsibilities

* Load topology definitions.
* Validate input.
* Reject malformed or invalid data.
* Construct runtime topology objects.

Validation should occur before invalid topology state is exposed to the rest of the simulation.

The loader must not silently accept invalid required fields.

---

## 12. Application / GUI Domain

The GUI domain presents and controls the simulator.

### Responsibilities

* Render the network.
* Display simulation state.
* Display statistics.
* Allow topology interaction.
* Expose configuration controls.
* Expose simulation controls.

The GUI must not duplicate core simulation logic.

---

## 13. GUI Interaction Domain

User actions must map to explicit domain operations.

For example:

```text
Drag node A
    ↓
Select node B
    ↓
Request connection
    ↓
Create / schedule TCP connection
    ↓
Simulation remains paused
```

Creating a connection must not implicitly mean:

```text
Start simulation
```

Simulation execution must remain under explicit simulation controls.

---

## 14. Testing Domain

Testing is treated as a first-class engineering domain.

It validates contracts across the other domains.

Testing includes:

* unit tests;
* integration tests;
* headless execution;
* test discovery.

The testing domain must verify observable behavior rather than implementation details whenever possible.

---

## 15. Domain Boundaries

The following dependency direction is preferred:

```text
GUI
 ↓
Simulation / Core
 ↓
Network domains
```

The reverse dependency should be avoided.

In particular:

```text
Network Core
    X
    ↓
GUI
```

is not an acceptable architectural dependency.

---

## 16. Cross-Domain Operations

Some operations necessarily cross domain boundaries.

Example:

```text
TCP connection
      ↓
Packet creation
      ↓
Routing
      ↓
Link selection
      ↓
Queue
      ↓
Event scheduling
      ↓
Packet delivery
      ↓
TCP state update
```

Cross-domain operations must preserve each participating domain's contract.

A higher-level operation must correctly propagate failures from lower-level operations.

---

## 17. Domain Change Rule

When modifying a component, first identify which domain owns the behavior.

Avoid placing behavior in a domain merely because that domain is convenient to access.

A change that affects multiple domains must explicitly consider the contracts between them.
