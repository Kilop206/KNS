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
The destination callback terminates each abstract message. TCP sessions
use a separate Analytics group namespace and payload-goodput accounting.

Generated packets have exactly one terminal outcome or remain pending. A shared
lifecycle record follows hop copies; source rejection, forwarding rejection and
TTL exhaustion finalize drops. The invariant is `generated = delivered + dropped
+ pending` per flow and globally. Queue overflow/RED/random loss use real model
decisions, not synthetic results. Sampling/cancellation can leave both generated
packets and future generation pending; reports distinguish them.

Limits: 256 flows, 1,000,000 generated packets globally, 10,000 offsets per custom
flow, 1 MiB per modeled wire packet and end times at most 1,000,000 seconds. Only
one source-generator event per active flow is queued. Optional responses add
processing events bounded by generated requests; network arrivals remain subject
to link queues. Reaching the generation budget truncates offered load
explicitly with `generation_limit_reached`; it is not a completed capacity test.
CLI input is capped at 4 MiB. The ledger retains scalar counters rather than a
history of all packets. Analytics retention is configured independently.

Tests cover analytical serialization/propagation, shared queue congestion,
source/intermediate/TTL drops, pending work/cancellation, budget truncation,
deadlines, isolated replayable random streams and atomic configuration rejection.
Arbitrary application dependency graphs, arbitrary distributions, trace replay
and Studio controls remain separate work.

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

## Experiment bundles

```powershell
python scripts/experiment_profile.py --profile "KNS Experiments/traffic-lab/profile.json" --exe build-core-mingw/verification-build/app/KNS.exe --output results/traffic-experiment
python scripts/experiment_profile.py --replay results/traffic-experiment/manifest.json --exe build-core-mingw/verification-build/app/KNS.exe --output "results/traffic replay"
```

Root `traffic_plan` embeds the full plan in the hashed configuration; generated
bundles retain it as `traffic.json` and require/validate every `.traffic.json`
outcome. The example changes only backbone bandwidth, comparing 1 with 10 Mbps.
Two network seeds keep the same independently seeded offered schedule. At zero
configured random loss their repeats may coincide; they do not create independent
workload samples. Vary the plan seed in separate saved profiles for that purpose.

TCP size/control sweep dimensions are rejected when they have multiple values;
the retained single legacy labels do not set abstract wire sizes or policies.
Compare preserves legacy CSV metric definitions, including first-hop admission
denominators; use the traffic ledger for generated-packet delivery/deadline ratios.
Cross-bundle comparisons record plan hashes and whether workload inputs match.
Different plans can be compared descriptively but do not establish a controlled
causal change. Traffic-rate/size sweeps are described below.

## Bounded traffic parameter sweeps

`sweep.traffic` combines up to 16 whitelisted flow parameters with link axes.
Each axis declares a `flow_id`, `parameter` and `values` list or inclusive
`{start, stop, step}` range. For example:

```json
{"bandwidth": [1, 10], "traffic": [
  {"flow_id": 2, "parameter": "schedule.rate_bps", "values": [600000, 1200000]},
  {"flow_id": 2, "parameter": "wire_bytes", "values": [500, 1000]}
]}
```

This produces eight variants before seed/routing dimensions. Existing `max_cases`
bounds both expansion and the final matrix. Fixed named variants may instead
declare `traffic_overrides` entries with `flow_id`, `parameter`, `value`. Supported
parameters include wire size, timing, class, hop TTL, packet deadlines, existing
schedule rates/intervals/burst sizes and existing application response size/delay/
operation deadline. Overrides cannot switch/create a schedule/application model,
modify endpoints or silently select an unknown flow. Every effective plan is
validated before creating output; the base plan is preserved.

```powershell
python scripts/experiment_profile.py --profile "KNS Experiments/traffic-lab/load-sweep.json" --exe build-core-mingw/verification-build/app/KNS.exe --output results/load-sweep
```

This smaller four-variant example crosses two backbone capacities with two CBR
rates. Bundles retain a plan per topology variant in `traffic-plans/`, its full
configuration in the hashed manifest and the resulting ledger for every case.
Compare fingerprints the effective plans, including overrides; a matched metadata
pair does not imply equal offered load when those fingerprints differ. Hold rate
fixed to investigate a bandwidth change and hold bandwidth fixed to investigate
load growth. Do not attribute a difference automatically when both change.

## Causal request/response application models

An optional flow `application` object names an abstraction and, where appropriate,
declares `response_wire_bytes`, `processing_delay_s` and optional operation
`deadline_s`. A response is scheduled only after its request reaches the destination
and its fixed processing delay elapses. It traverses the current reverse route,
sharing real link queues and loss decisions. Responses may drain after source
`end_s`; the global packet budget includes requests and responses. Source schedules
count source messages independently, so replies never skip future source entries.

| Kinds | Declared communication model | Important omissions |
| --- | --- | --- |
| `request_response`, `http`, `api`, `database`, `microservice` | Request, fixed processing delay, response with configured wire sizes | Protocol parsing, TCP, connection setup, query/service logic, dependency graphs |
| `messaging` | One-way message or optional request/acknowledgement pair | Broker semantics, durable storage, retry and delivery guarantees |
| `telemetry` | One-way or optional paired message using chosen schedule | MQTT/industrial protocol semantics and QoS |
| `streaming` | Offered wire stream or optional paired messages | Playback, codecs, congestion control and adaptive rates |

The kind labels select/validate these abstractions; they do not infer real payload
sizes or processing costs. Processing is a timer with unlimited concurrency, not a
device CPU, finite-capacity worker pool or processing queue. One operation is one
source request and its response. There are no implicit retries.

Flow reports separate source/response generation, delivery/drop stages, operations
started/completed/failed/pending, operation deadline outcomes and round-trip latency
statistics. Failed requests/responses end the operation. Budget-truncated replies
leave operations pending and the run incomplete. Moments/histograms cover completed
operations; quantiles retain at most 256 recent completions. Neither completions
nor retained percentiles establish a service SLA. Flow `deadline_s` remains a
per-message deadline; `application.deadline_s` covers request creation through
response arrival, including modeled processing.

Run the mixed application example with the same topology/Analytics options and
`--traffic-plan "KNS Experiments/traffic-lab/applications.json"`. It combines web,
API, database, microservice, messaging, telemetry and reverse streaming messages
on the shared backbone. Inspect operation latency, failed operations and deadline
counts alongside the one-way streaming load; compare a controlled bandwidth
upgrade before inferring a bottleneck. The existing simulated HTTP/DNS service
runtime is separate from these generic wire-message models.
