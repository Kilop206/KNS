# KNS — Data Model

This document describes the principal runtime entities and relationships in KNS.

The exact class definitions in the repository remain authoritative. This document defines the conceptual model and the contracts that the implementation should preserve.

---

## 1. Entity Overview

The principal entities are:

```text
Topology
├── Node
└── Link

Simulation
├── Event
└── Simulation State

Network
└── Packet

TCP
├── TCP Connection
└── TCP Session
```

---

## 2. Topology

The topology represents the simulated network graph.

Conceptually:

```text
Topology
 ├── Nodes
 └── Links
```

A topology owns or otherwise manages the relationships between its network entities according to the implementation's ownership model.

The topology is the authoritative source for current connectivity.

---

## 3. Node

A node represents a network endpoint.

Conceptually:

```text
Node
├── Identity
├── Network relationships
└── Protocol participation
```

Nodes may participate in:

* routing;
* packet transmission;
* TCP connections;
* topology visualization.

Node identifiers must be unambiguous within the topology.

---

## 4. Link

A link connects two nodes.

Conceptually:

```text
Link
├── Endpoint A
├── Endpoint B
├── Operational state
├── Transmission characteristics
└── Queue / capacity state
```

A link may be operationally:

```text
UP
DOWN
```

A `DOWN` link represents an unavailable network path for normal routing and transmission.

---

## 5. Packet

A packet represents a unit of simulated network traffic.

Conceptually:

```text
Packet
├── Source
├── Destination
├── Protocol information
├── Payload / control information
└── Transmission context
```

The exact fields depend on the packet implementation.

### Identity

Packet identity must be sufficient to distinguish packets when multiple packets coexist in the event queue or network.

### Transmission context

When required by the simulation, a packet or its associated event must preserve the identity of the transmission/link through which it was sent.

This prevents mutable topology state from making an in-flight packet ambiguous.

---

## 6. Event

An event represents a scheduled unit of simulation work.

Conceptually:

```text
Event
├── Simulation timestamp
├── Ordering information
└── Action / event payload
```

Events are processed according to logical simulation time and the ordering rules established by the implementation.

Events must contain enough information to execute correctly when they become due.

---

## 7. Simulation State

The simulation lifecycle is represented conceptually by:

```text
Ready
Running
Paused
Finished
```

The state describes execution lifecycle rather than network connectivity.

The distinction is important:

```text
No events before Start
        ≠
Finished simulation
```

`Finished` should represent completion after simulation execution has exhausted its work.

---

## 8. TCP Connection

A TCP connection represents protocol state between communicating endpoints.

Conceptually:

```text
TCPConnection
├── Local endpoint
├── Remote endpoint
├── Connection state
├── Sequence information
├── Acknowledgement information
└── Timers / expiration state
```

The exact data members are implementation-defined.

---

## 9. TCP State

TCP state represents the current protocol phase.

The model includes behavior corresponding to:

```text
Connection establishment
        ↓
Established communication
        ↓
Termination
        ↓
TIME_WAIT
```

The exact enumeration and state graph must follow the implementation.

---

## 10. TCP Session

A session may represent a higher-level grouping of TCP connection activity.

Conceptually:

```text
TCPSession
└── TCP connection(s)
```

If session-level state is exposed, it must have a clearly defined relationship to the underlying connections.

---

## 11. Relationships

The principal relationships can be represented as:

```text
Topology
 │
 ├──────────────┐
 ▼              ▼
Node           Link
 ▲              │
 │              │
 └──────────────┘

Node
 │
 ├── Packet
 │
 └── TCPConnection

Packet
 │
 ▼
Event

TCPConnection
 │
 ├── Control packets
 └── Data packets
```

---

## 12. Ownership

Ownership and lifetime must follow the actual implementation.

Documentation must not claim ownership semantics that are not present in the code.

When introducing new entities, explicitly define:

* owner;
* lifetime;
* reference semantics;
* mutation authority;
* destruction behavior.

---

## 13. Identity and References

References between simulation entities must remain valid for the lifetime in which they are used.

Special care is required when:

* nodes are removed;
* links are removed;
* topology changes dynamically;
* events remain queued;
* packets remain in transit.

An event must not blindly dereference an entity that may no longer exist.

---

## 14. Invalid State

Invalid states must be rejected or explicitly represented.

Examples include:

* unknown node identifiers;
* invalid link endpoints;
* invalid next-hop requests;
* packets sent through unavailable links;
* invalid TCP transitions.

The appropriate failure mechanism depends on the API contract.

---

## 15. Data Model Evolution

Changes to entity structure should consider:

1. existing serialized data;
2. existing tests;
3. event compatibility;
4. topology mutation;
5. TCP state;
6. GUI consumers.

Data-model changes should be accompanied by relevant tests and documentation updates.
