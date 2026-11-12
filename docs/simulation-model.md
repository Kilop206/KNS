# Simulation model and execution control

All times in the engine API are virtual seconds. JSON link delay is milliseconds;
bandwidth is megabits/second. A packet of B serialized bytes occupies a transmitter
for `8 * B / (bandwidth_mbps * 1e6)` seconds. Its arrival is the next available
transmission start plus serialization plus propagation. Queued transmission
starts serialize in the permitted duplex direction. Queue capacity currently
counts transmissions through arrival, not just packets waiting for service.

The scheduler owns events until dispatch, ordering by timestamp then construction
ID. An event cannot be created at a negative/nonfinite time or scheduled before
the current clock. Routing changes invalidate cached tables synchronously.
Accepted packets finish their hop across link failure/removal. These assumptions
are simulation models, not real network measurements or convergence predictions.

## Bounded execution example

```cpp
kns::Topology topology(3);
topology.addLink(0, 1, 10.0, 2.0);
topology.addLink(1, 2, 10.0, 2.0);
kns::SimulationEngine engine(topology);
kns::RunConfig config;
config.seed = 42;
engine.configureRun(config);
engine.startTCPConnection(0, 2);
auto result = engine.runUntil(0.01, 10000);
engine.pause();
engine.step(); // one pending event, leaving the engine paused
engine.resume();
engine.run(); // drain remaining events; legacy API retained
```

`runUntil(horizon_s, max_events)` includes all events at the horizon, including
newly scheduled ties, subject to the event budget (default one million). It
returns a stop reason, completed event count for that call, current virtual time
and pending count. If the next event is later than the horizon, the clock advances
to the horizon without dispatching it. If the queue drains first, time stays at
the last event; an empty run does not invent an observation interval. Invalid
horizons (nonfinite or earlier than now) throw without mutation. Zero budget
dispatches nothing; when no event is eligible, queue/time stop takes precedence.
An event budget bounds callbacks that continually schedule zero-delay work,
but cannot interrupt an individual callback that never returns.

`pause()` prevents `run`, `runUntil` and `processEvent` from dispatching until
`resume()`. `step()` explicitly dispatches one event even while paused. `cancel()`
is terminal; a callback already executing finishes, including any follow-up
scheduling needed for its bookkeeping, then execution stops. Scheduling new work
from outside that active callback is rejected after cancellation.
Cancelled engines retain events, sessions, queues and in-flight records as an
unfinished diagnostic snapshot. Create a new engine to restart. No pending
packet is silently converted into delivered or lost by cancellation. Event
cancellation remains available by ID through `cancelEvent`.

Callback exceptions propagate, increment failed-event statistics and terminally
mark the engine Failed. Pending events remain inspectable. Recursive execution
from a callback throws. All APIs require one owner thread; controls are cooperative,
not thread-safe signalling. GUI integration of these new controls is pending.

The headless CLI exposes `--until <seconds>` and `--max-events <integer>`:

```powershell
.\build-core-mingw\app\KNS.exe --headless --topology app/topologies/mesh4.json --until 0.01 --max-events 10000 --output "results/partial run.csv"
```

If pending work remains at a limit, exit code `3` and a JSON diagnostic on stderr
mark the CSV as incomplete. It remains the existing aggregate CSV schema and
must be kept with its exit status; it is not a replay/checkpoint file. A drained
run follows existing validation and exit codes. Supplying neither option keeps
the legacy drain behavior. Both options require headless mode. The integration
CTest `headless_execution_limits` verifies argument rejection, partial snapshots,
paths with spaces and completion under generous limits.

`executionStatistics()` counts dispatched, completed and failed callbacks for
the engine lifetime. No-op/stale TCP callbacks count as completed events. Cancelled
pending events never count as dispatched. These counters are not packet metrics.

## Reproducibility and verification

Equivalent build, topology, configuration, seed and logical event order reproduce
the run. Cross-platform floating-point or standard-library random-distribution
bit identity is not guaranteed. Slicing a run does not reseed or change event
order. The `[execution]` Catch2 tests exercise boundary ties, zero-time recurrence,
pause/step/resume, cancellation, exception/reentrancy behavior, and compare a
seeded multihop TCP workload run continuously and in slices. Existing link timing
tests verify serialization and propagation against analytical calculations.

Packet/byte ledgers, link utilization and unique application goodput remain
incomplete; existing packet counters must not be represented as those metrics.
