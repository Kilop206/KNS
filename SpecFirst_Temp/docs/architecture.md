# KNS — Architecture

## 1. Architectural Goal

KNS is organized to keep simulation logic independent from graphical presentation and user interaction.

The primary architectural boundary is:

```text
┌───────────────────────────────┐
│        Application / GUI      │
│                               │
│ ImGui / GLFW / OpenGL         │
│ User interaction              │
│ Configuration                 │
│ Visualization                │
└───────────────┬───────────────┘
                │
                ▼
┌───────────────────────────────┐
│           KNS Core            │
│                               │
│ Simulation                    │
│ Topology                      │
│ Links                         │
│ Packets                       │
│ Routing                       │
│ TCP                           │
│ Event scheduling              │
└───────────────┬───────────────┘
                │
                ▼
┌───────────────────────────────┐
│            Tests              │
│                               │
│ Unit tests                    │
│ Integration tests             │
│ Headless validation           │
└───────────────────────────────┘
```

---

## 2. Core Layer

The core contains the domain model and simulation behavior.

It must not depend on GUI libraries.

The core is responsible for:

* simulation time;
* event scheduling;
* topology;
* nodes;
* links;
* packets;
* routing;
* TCP;
* transmission behavior;
* state transitions.

The core should remain usable independently of graphical execution.

---

## 3. Application / GUI Layer

The application layer connects user interaction and visualization to the core.

It is responsible for:

* creating and configuring the simulation;
* rendering topology;
* exposing simulation controls;
* displaying state;
* receiving user input;
* invoking core operations.

It must not reimplement core simulation behavior.

---

## 4. Simulation Engine

The simulation engine manages logical execution.

Its responsibilities include:

* maintaining the event queue;
* maintaining simulation time;
* processing scheduled events;
* controlling execution state;
* starting, pausing, resuming, stepping, and finishing execution.

Simulation lifecycle state is explicit.

```text
READY
  ↓
RUNNING
  ↓
PAUSED
  ↓
RUNNING
  ↓
FINISHED
```

---

## 5. Event Model

Simulation behavior is represented through discrete events.

Events should contain sufficient information to execute their intended operation without depending on unstable external state.

Event processing must preserve deterministic ordering.

Changes to event scheduling must therefore consider:

* timestamp;
* ordering;
* event identity;
* affected network entities;
* topology changes;
* cancellation or invalidation behavior.

---

## 6. Topology

The topology represents the network graph.

Conceptually:

```text
Topology
├── Nodes
└── Links
```

Topology responsibilities include:

* node management;
* link management;
* lookup;
* connectivity;
* validation;
* topology mutation.

The topology is the authoritative representation of network connectivity.

---

## 7. Links

A `Link` represents a connection between network nodes.

Links may have operational states such as:

```text
UP
DOWN
```

A `DOWN` link must not be considered available for normal routing or transmission.

Links may also impose transmission constraints such as queue or capacity limits.

---

## 8. Routing

Routing operates over the current topology.

Routing must account for the operational state of links.

The routing layer must not return a path that depends on an unavailable link unless such behavior is explicitly part of the API contract.

Next-hop APIs must define:

* valid source;
* valid destination;
* valid node identifiers;
* unreachable behavior;
* bounds;
* failure semantics.

---

## 9. Packet Transmission

Packet transmission crosses several architectural boundaries:

```text
Packet
  ↓
Routing
  ↓
Link selection
  ↓
Queue / capacity
  ↓
Scheduled transmission
  ↓
Destination
```

A transmission must distinguish between:

* packet identity;
* transmission event;
* originating link;
* current topology state.

A packet already in transit must not become ambiguously associated with a different link merely because the topology changed.

---

## 10. TCP

The TCP subsystem models simplified connection behavior.

Its conceptual lifecycle is:

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

TCP state must be explicit and observable.

TCP connection state and aggregate session state must not contradict each other.

Failure transitions must have defined semantics.

---

## 11. GUI and Simulation Separation

A critical rule is:

> User interaction must not implicitly redefine simulation execution semantics.

For example:

```text
Dragging node A onto node B
        ↓
Create / schedule TCP connection
        ↓
Simulation remains PAUSED
```

The interaction itself must not cause the simulation to begin processing queued events.

Simulation execution occurs through explicit controls.

---

## 12. Dynamic Topology

Topology can potentially change while events are already scheduled.

This introduces consistency questions involving:

* routes already computed;
* packets already in transit;
* links that become unavailable;
* nodes or links removed;
* stale references;
* events referencing obsolete entities.

The architecture must define the behavior of each affected operation rather than relying on accidental behavior.

---

## 13. Error Handling

Architectural APIs should make failure observable.

Examples:

```text
sendPacket()
    ↓
accepted / rejected

getNextHop()
    ↓
valid next hop / unreachable / invalid request

Topology mutation
    ↓
success / rejected invalid operation
```

Higher-level operations must propagate or correctly interpret lower-level failures.

A wrapper must not report success merely because the wrapper itself executed.

---

## 14. Testing Architecture

Tests should exercise the architecture at appropriate levels.

### Unit tests

Verify individual components and contracts.

### Integration tests

Verify interactions between components.

### Headless tests

Verify complete simulation behavior without GUI dependencies.

The GUI should not be required to validate core simulation correctness.

---

## 15. Dependency Direction

The intended dependency direction is:

```text
GUI / Application
        ↓
KNS Core
```

not:

```text
KNS Core
        ↓
GUI
```

Core code must remain portable and independently testable.

---

## 16. Architectural Change Rule

Any change that alters one of the following requires architectural review:

* ownership;
* dependency direction;
* event lifecycle;
* topology authority;
* packet identity;
* simulation lifecycle;
* TCP state model;
* GUI/core boundary.

Small implementation changes that remain inside an established boundary do not require architectural redesign.

---

## 17. Architectural Objective

The architecture should make the following properties easy to maintain:

```text
Deterministic
Testable
Observable
Modular
Explicit
Maintainable
```

The simulator should behave as a coherent event-driven system rather than as a collection of GUI-triggered side effects.
