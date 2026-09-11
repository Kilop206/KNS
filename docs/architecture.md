# KNS Architecture and Design Decisions

## System boundary

KNS separates a reusable simulation library from application concerns:

```text
app (GUI and headless CLI)
        │
        ▼
core simulation library
├── engine: clock, events, sessions, listeners, and statistics
├── network: topology, links, packets, and routing
└── transport/tcp: endpoint state, buffers, timers, and recovery
```

`core/` does not depend on Dear ImGui, GLFW, or OpenGL. The application may
observe and configure the engine, but protocol and routing behavior remains in
the library so it can be tested without creating a window.

## Deterministic event scheduling

`EventQueue` is a priority queue of `std::unique_ptr<Event>`. Events are ordered
first by their `double` timestamp in simulated seconds and then by a monotonic
64-bit event ID. The ID preserves insertion order when timestamps match.

A priority queue avoids scanning a list for every dispatch and gives the
expected `O(log n)` insertion/removal behavior. Exclusive ownership also makes
event lifetime explicit: the queue owns a pending event, and the engine owns it
only while executing it.

Determinism assumes the same topology, configuration, random seed, and event
insertion order. It does not imply bit-identical floating-point results across
different compiler or hardware implementations.

## Simulation ownership

`SimulationEngine` owns:

- the logical clock and event queue;
- a topology snapshot and routing tables;
- packet statistics and in-flight packet records;
- TCP sessions and passive listeners;
- observers used by the GUI.

Events reference sessions by stable numeric ID. Before accessing a session,
events that can outlive it call `hasTCPSession()`. Cancelling a session therefore
does not require searching and removing every pending event.

## Topology and routing

`Topology` stores nodes, interfaces, shared link objects, and adjacency lists.
Links have stable IDs so parallel links between the same nodes can be mutated or
removed unambiguously.

Routing tables are built with Dijkstra. Four metrics are supported:

- sum of propagation delay;
- maximum bottleneck bandwidth;
- hop count;
- sum of delay divided by bandwidth.

Tables are not recomputed per packet. Engine topology APIs rebuild them
synchronously. Direct mutations through `SimulationEngine::getTopology()` or a
contained `Link` increment a shared routing revision; `getNextHop()` and
`getRoutingTable()` refresh stale tables before returning. This preserves a safe
mutable API while keeping the steady-state route lookup inexpensive.

## Dynamic topology policy

Topology changes use these invariants:

1. A packet already accepted by a link completes that hop even if the link is
   subsequently removed.
2. New transmissions observe current link availability and current routes.
3. A packet that reaches an intermediate node with no route is counted as lost.
4. Removing a node leaves an inactive slot, preserving existing node IDs.
5. TCP sessions are not automatically deleted merely because a route becomes
   unavailable; their protocol timers determine subsequent behavior.

The stable link ID stored in `PacketTravelInfo` lets arrival release the exact
parallel link that carried the packet.

## Transport boundary

TCP builds segments and owns endpoint protocol state, but it does not select or
reserve links. `PacketUtils` bridges protocol packets to the network layer, and
`SimulationEngine::sendPacket()` applies serialization, propagation delay,
queue capacity, direction, and loss.

This boundary keeps routing and physical-link policy independent from the TCP
state machine. See [`tcp_design.md`](tcp_design.md) and
[`protocol_spec.md`](protocol_spec.md) for the implemented transport subset.

## Error and validation strategy

- Invalid construction/configuration values throw `std::invalid_argument`.
- Lookup-style mutations return `false` when their target is absent or
  ambiguous.
- Unreachable routes return next hop `-1`.
- Passive TCP acceptance uses `TCPListener::INVALID_SESSION_ID` as its failure
  sentinel; real session IDs begin at zero.
- Headless execution returns non-zero for invalid arguments, topology loading
  failures, or failed simulation validation.

These contracts make failure visible without relying on debug logging.

## Intentional constraints

The core is single-threaded. Logical time is independent from wall-clock time;
the GUI controls presentation speed without changing event timestamps. Routing
is centralized rather than modeled as a distributed protocol. The TCP layer is
a deterministic experimental subset, not a kernel socket implementation.
