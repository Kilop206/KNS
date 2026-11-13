# KNS Architecture and Design Decisions

## Platform evolution (2026-11-12)

The target dependency direction is applications (Studio, independent CLI,
optional local API) -> experiments/validation/reporting -> analytics/traffic/
industrial models -> core engine/network models. Infrastructure provides
serialization and diagnostics. Optional Linux emulation is a separate adapter,
never an implicit execution path of the discrete-event engine.

Preserve the existing `KNS::kns` library, `KNS` application and `kns_tests`
targets. Currently headless execution shares the graphical application's build
dependencies; splitting its target remains necessary. `SimulationEngine` also
owns TCP lifecycle and network services, so adding all platform orchestration
there would worsen existing coupling. Experiment runners should own engines,
feed configuration before scheduling, and consume observations afterward.

Execution contracts: all engine mutations occur on one owner thread; virtual
time uses seconds and never follows wall time. A bounded run includes events
at its horizon, leaves later events pending, and has an event budget to stop
zero-time recurrence. Pause preserves pending work; terminal cancellation
preserves a diagnostic snapshot rather than pretending packets were delivered.
Exception failures must be observable and cannot be resumed as successful runs.
Scheduler counts describe dispatched/completed/failed events separately from
packet metrics. Existing event IDs order equal timestamps by construction order,
not necessarily scheduling order when callers construct events in advance.

Future scenario contracts must include format version, topology, seed, traffic,
duration, failures, assertions and output policy. Reuse existing JSON topology
loaders and Python profile/comparison tooling. Version result schemas before
changing counter meanings. Distinguish simulated packets from unique application
payload: TCP retransmissions and control traffic cannot be counted as goodput.

The current link queue retains slots until arrival (including propagation),
which is a model assumption requiring explicit review before claims about
physical waiting-buffer occupancy. Existing accepted in-flight packets finish
their hop after link removal. Routing refresh is synchronous; this is not a
model of distributed routing convergence.

See [ROADMAP.md](ROADMAP.md) and [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)
for acceptance gates and local evidence.

The first milestone 2 increment uses the existing Python orchestration boundary:
`experiment_profile` validates/normalizes inputs, `experiment_sweep` bounds and
expands link variants, `experiment_manifest` persists portable inputs and hashes,
and `benchmark_suite` runs the actual CLI and records each case. Comparison remains
in `compare_experiments`. The engine and aggregate CSV contract are unchanged.
Replay reconstructs a new suite from embedded manifest configuration; it never
executes a command supplied by the manifest or depends on its old absolute paths.

`engine/experiments/FaultLab` schedules validated link changes and observations as
ordinary owned events. It retains a shared per-run report, never wall-clock timers
or UI state. Existing link setters invalidate routing; accepted packet events keep
their timing. Probe route traces use original link indices in reports so process
global identities do not leak into deterministic replay artifacts. Python plans
are preflighted before generation and the native Core validates them independently
before scheduling. The benchmark runner verifies applied events and observations
before accepting a fault case's output. See [fault-injection.md](fault-injection.md).

Compare stays outside the engine: `compare_experiments` validates and pairs saved
records, `comparison_statistics` computes descriptive statistics/conditional
intervals, `comparison_artifacts` declares cross-bundle selections and routing
pairs, and `comparison_html` renders escaped static HTML/SVG. No rendering or
statistical package becomes a Core dependency. Per-pair settings and provenance
remain visible; absent metrics and failed cases do not become invented samples.

Runtime Analytics is an optional injected `MetricsCollector`, separate from GUI
packet/latency observers. The engine reports admitted serialization intervals and
destination packet deliveries; ordinary sampling events observe virtual-time
state. `LatencyStatistics` owns stable moments, all-observation histogram counts
and bounded recent quantile storage. Collector internals bound pending reservations,
time samples and group retention; unavailable utilization is null. Profile/batch
orchestration persists configuration and requires corresponding report evidence.
No random stream, network operation or LLM is added. See [analytics.md](analytics.md)
for scopes, definitions, rules and incomplete model coverage.

The collector now tracks previous DATA latency per retained flow, so latency-change
pairs never cross session/direction boundaries. Joint serialization/queue-pressure
streaks retain scalar evidence independently of time-series trimming. Saved reports
pass configuration/schedule checks in `analytics_config` and statistical/conservation
checks in `analytics_validation`. The optional `analytics_report` exporter reads
completed native reports and creates a new local directory without rerunning the
simulator. Legacy reports without variation remain readable with that metric absent.

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
64-bit event ID. The ID preserves construction order when timestamps match.

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
6. Changing link mode while its queues contain transmissions throws
   `std::logic_error`; the old mode and queued arrivals remain intact. Once
   drained, mode changes invalidate routing just like availability changes.
7. Queue capacity defaults to 32, is positive and configurable, and cannot be
   reduced below occupancy. FULL_DUPLEX capacity is per direction; HALF_DUPLEX
   capacity is shared. SIMPLEX permits only A-to-B routes and transmissions.

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

TCP creation and listening APIs reject nonexistent or inactive nodes before
allocating session IDs, inserting listeners, or scheduling handshakes. Packet
sizes must be positive, including packets passed directly to transmission APIs.

`configureRun()` applies `RunConfig::packet_size` and reseeds the random source
before sessions or events exist. The CLI exposes both packet size and seed;
GUI restarts apply the configuration again. The random source is currently
process-global, so deterministic runs must execute sequentially rather than
interleave independently configured engines.

Simulation validation separates the descriptive `loss_free` metric from
integrity. Successful validation requires completed TCP workloads with both
endpoints closed and buffers empty, no scheduled work, no packets in transit,
and empty link queues. Delivered packets cannot exceed sent packets, and
undelivered sends must be accounted for by losses. Losses may exceed that gap
because rejected sends also increment the loss counter.

## Intentional constraints

The core is single-threaded. Logical time is independent from wall-clock time;
the GUI controls presentation speed without changing event timestamps. Routing
is centralized rather than modeled as a distributed protocol. The TCP layer is
a deterministic experimental subset, not a kernel socket implementation.
