# Traffic Lab: abstract workloads

`traffic::TrafficRun::schedule(engine, plan)` schedules bounded concurrent flows
through the existing routing/link/queue models. Attach before execution. It is
independent of the GUI, services and TCP. The headless `--traffic-plan plan.json`
option replaces automatic TCP generation and writes `<output>.traffic.json`.
It can be combined with `--analytics`, `--fault-plan`, `--until` and `--max-events`.

Each flow declares source/destination node indices, wire bytes, start/end seconds
and a schedule. All times are virtual; the end is exclusive. Periodic/CBR/burst
flows emit at their start. Poisson/uniform flows first wait a sampled interval.
Custom offsets are relative to start and may tie; scheduler ordering remains
stable. CBR spacing is `8 * wire_bytes / rate_bps`; it describes offered wire load,
not guaranteed throughput. Bursts have explicit packets, packet spacing and cycle
spacing, with no overlap between bursts.

Poisson uses exponential interarrival times; uniform intervals have declared min
and max. Every flow owns an `mt19937_64` stream derived from plan seed and numeric
flow ID using fixed SplitMix64 arithmetic. Adding/reordering another flow does not
consume this stream. Link loss/RED retain the engine seed separately. Reproduction
requires both seeds and equivalent engine/version/environment. Floating-point
logarithms are not promised identical across different math-library environments;
intervals below clock resolution advance to the next representable instant.

The current profiles are `periodic`, `cbr`, `burst`, `poisson`, `uniform`, and
`custom`. Multiple directions and different start/end times express asymmetric,
short and long flows. Classes 0–7 are recorded metadata; existing FIFO/RED link
policies apply and priority does not grant scheduling preference. Configurable
TTL counts forwarded hops. Optional `deadline_s` is relative to each generation;
late packets are delivered and counted late, not silently discarded.

Wire size includes whatever headers a workload designer chooses to model. These
packets do not implement UDP, HTTP, MQTT, reliable transport or application reads.
The current destination callback terminates the abstract message. TCP sessions
use a separate Analytics group namespace and payload-goodput accounting.

Generated packets have exactly one terminal outcome or remain pending. A shared
lifecycle record follows hop copies; source rejection, forwarding rejection and
TTL exhaustion finalize drops. The invariant is `generated = delivered + dropped
+ pending` per flow and globally. Queue overflow/RED/random loss use real model
decisions, not synthetic results. Sampling/cancellation can leave both generated
packets and future generation pending; reports distinguish them.

Limits: 256 flows, 1,000,000 generated packets globally, 10,000 offsets per custom
flow, 1 MiB per modeled wire packet and end times at most 1,000,000 seconds. Only
one generator event per active flow is queued; network arrival events remain
subject to link queues. Reaching the generation budget truncates offered load
explicitly with `generation_limit_reached`; it is not a completed capacity test.
CLI input is capped at 4 MiB. The ledger retains scalar counters rather than a
history of all packets. Analytics retention is configured independently.

Tests cover analytical serialization/propagation, shared queue congestion,
source/intermediate/TTL drops, pending work/cancellation, budget truncation,
deadlines, isolated replayable random streams and atomic configuration rejection.
Application dependency models, arbitrary distributions, traffic-plan sweeps,
trace replay and Studio controls remain separate work.

## Run the shared-backbone example

```powershell
.\build-core-mingw\verification-build\app\KNS.exe --headless --topology "KNS Experiments/traffic-lab/topology.json" --traffic-plan "KNS Experiments/traffic-lab/traffic.json" --analytics "KNS Experiments/traffic-lab/analytics.json" --seed 42 --output "results/traffic/run.csv"
python scripts/analytics_report.py "results/traffic/run.csv.analytics.json" --output "results/traffic/view"
```

Use the path to your own build's `KNS.exe` when different. Six offered schedules
compete on a 1 Mbps backbone with eight queue slots. Compare delivered/dropped
and deadline counts by flow with serialization pressure in Analytics. Reverse
traffic uses the separate full-duplex slot. Change backbone bandwidth to 10 Mbps
and rerun with the same two seeds to investigate a controlled upgrade; do not
infer the effect of priority scheduling from class metadata alone.

The versioned schema is `contracts/traffic-plan-v1.schema.json`; runtime preflight
also validates endpoint existence, unique IDs and cross-field timing constraints.
CLI codes: 0 means the workload drained, including legitimate all-loss/zero-load
cases; 1 means invalid input/execution error; 3 means a time/event/generation budget
or pending work left the experiment incomplete. Delivery targets require separate
assertions. JSON records both traffic and network seeds; raw traffic/Analytics
reports replay identically under equivalent conditions.
