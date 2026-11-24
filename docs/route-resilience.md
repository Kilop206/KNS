# Route resilience

The canvas can preview single-cable outages without changing the running
simulation. It checks the selected **directional route**, lists critical cables,
and compares the propagation delay and bottleneck capacity of alternative routes.

## Try the lab

Open `app/topologies/resilience-lab.json`, select **Route**, click Client (#0)
then Application server (#3), and click **Resilience** beside the route summary.

The delay-based baseline has two hops, 2 ms of configured propagation delay and
100 Mbps bottleneck capacity:

- Losing Client--Edge router disconnects this pair. Its row is critical.
- Losing Edge router--Application server uses Backup router instead. The
  alternative has three hops, 8 ms propagation delay (+6 ms) and 20 Mbps capacity.

Select a row to highlight the failed cable in orange and its alternative in
purple. Green shows the baseline path; purple takes precedence on shared hops.
Click the row again or **Clear preview** to remove the overlay. Closing the
resilience window removes the preview. Escape or choosing another canvas tool
clears the selected route.

Changing a routing metric, cable state/configuration or device forwarding role
automatically restarts the analysis and clears the previous preview. Moving
devices preserves the results. Analysis proceeds one outage per visible frame,
checking up to 64 baseline cables. Longer paths explicitly show partial results;
unchecked cables are never reported as protected. Unreachable baselines and
local routes have their own explanatory states.

## What the results mean

Each case excludes exactly one stable link ID in both permitted directions, then
follows the simulator's converged hop-by-hop routing decisions. The same metric,
tie ordering, device forwarding restrictions, active nodes, simplex direction
and existing down links apply. Parallel cables are independent failure targets.
A loop is reported separately and counts as an outage without a reachable
alternative. Only baseline cables need testing: excluding any other cable
leaves the current forwarding path available.

This is configuration analysis. Capacity is the minimum configured link
bandwidth, not measured throughput; delay excludes serialization and queueing.
The preview does not model packet loss, transient routing convergence, deadlines,
TCP recovery or simultaneous failures. A reachable alternative does not imply
successful delivery or an SLA. The outage is never applied to live links or
queues and does not advance time, dispatch events or consume random numbers.
Use [Fault Lab](fault-injection.md) for scheduled outages with traffic evidence.

## C++ API

```cpp
#include "analysis/RouteResilience.hpp"

kns::analysis::RouteResilience analysis(engine, source, destination);
while (analysis.advance(engine)) {
    // Each call adds one LinkOutagePreview: failed_hop + alternative RouteTrace.
}
if (analysis.complete()) {
    auto critical = analysis.criticalCount();
    // All baseline cables checked; critical is a count for this directional pair.
}
```

The case limit defaults to 64 and accepts 1..256. `finished()` includes truncated
or unavailable analyses; `complete()` requires a reachable baseline and every
baseline hop checked. A reachable self-route is complete with zero cases.
`advance()` rejects a changed topology/metric or a different engine; reconstruct
the analysis when `matches(engine, source, destination)` becomes false. As with
the engine, cross-thread topology mutation is unsupported. Session objects must
not outlive their engine. A single `advance()` still performs routing work
proportional to topology size and alternative path length.

For an individual hypothetical outage, call
`engine.traceRoute(source, destination, excluded_link_id)`. An absent link ID
has no effect. The two-argument form retains the existing cached route tracing.
