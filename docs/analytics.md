# Runtime Analytics

The optional native collector measures simulated transmissions and destination
deliveries without consuming random numbers or replacing GUI observers. It
records bounded virtual-time series, DATA latency statistics, flow/destination
groups, directional link utilization and deterministic diagnostic recommendations.
It is independent of ImGui and does not require an LLM or external service.

## Run and replay

```powershell
python scripts/experiment_profile.py --profile "KNS Experiments/analytics-lab.json" --exe build-core-mingw/app/KNS.exe --output results/analytics
python scripts/experiment_profile.py --replay results/analytics/manifest.json --exe build-core-mingw/app/KNS.exe --output "results/analytics replay"
```

The example compares a 100 Mbps control with a 0.1 Mbps/two-slot link under the
same automatic TCP workload and two seeds. It collects through virtual second 1
at 5 ms intervals. Inspect case outcomes, observation counts and evidence before
using a recommendation. A short window with few DATA deliveries cannot establish
an application latency SLA or a network's maximum sustainable capacity.

Profiles accept an optional root `analytics` configuration, shared by all variants.
It is embedded in the hashed manifest and written as `analytics.json` in the bundle.
The benchmark runner requires the corresponding report and validates its config,
sample schedule/retention and completion before accepting an outcome. Each case
records `analytics_report` alongside raw CSV, logs and any Fault Lab report.

For direct headless use, save this configuration and pass `--analytics config.json`:

```json
{
  "schema_version": "1.0",
  "until_s": 1,
  "interval_s": 0.005,
  "max_samples": 256,
  "latency_retention": 4096,
  "max_groups": 128,
  "group_retention": 128,
  "max_reservations": 65536
}
```

```powershell
.\build-core-mingw\app\KNS.exe --headless --topology app/topologies/services-lab.json --analytics config.json --seed 42 --output "results/observed run.csv"
```

This writes `observed run.csv.analytics.json` beside the unchanged aggregate CSV.
`--until`/`--max-events` can interrupt execution; code 3 and pending events indicate
an incomplete report. A config error returns 1 before simulation/export. In C++,
use `analysis::MetricsCollector::attach(engine, config)` once before executing or
transmitting traffic; retain its shared pointer to call `toJson()` afterward.

## Measurement definitions

All timestamps and latencies use virtual seconds. Samples occur at zero, interval
multiples below the horizon, and the exact horizon. The interval count is limited
to 10000. Sampling follows timestamp/event-ID ordering, so a packet tied with a
sample can execute afterward. Window latency statistics cover deliveries since
the previous sampling callback. Global collector aggregates include DATA delivered
at time <= horizon, including horizon-tied deliveries after the last callback;
the last sample can consequently differ from the final collector aggregate.

The collection horizon limits observation, not the whole run. Simulation can
continue afterward while the collector ignores later deliveries/transmissions.
Observation events can also extend an otherwise drained run to the horizon,
changing final duration/aggregate throughput. They do not change packet event
order or RNG draws; choose identical observation settings for comparisons.

| Output | Definition |
| --- | --- |
| `throughput_pps` | Difference of destination packet-delivery counters / sample duration; includes control traffic. |
| `first_hop_admission_pps` | Difference of the legacy first-hop sent counter / duration; not generated application arrivals. |
| `drops_per_s` | Difference of aggregate path-drop counters / duration; not a loss probability. |
| `wire_bytes_delivered_in_scope` | Serialized destination bytes observed within the horizon, including control/retransmission bytes. |
| `data_latency` | Destination arrival time minus packet creation time for packets classified DATA by the existing engine. |
| `flow_groups` | Delivery aggregates keyed by session ID, source, destination and TCP ports; directions remain separate. |
| `destination_node_groups` | Delivery aggregates by destination node ID; not device processing/CPU statistics. |

Latency is network packet latency, not application request/response duration.
TCP duplicates/retransmissions and the existing service-frame DATA classification
are retained. Wire bytes and delivered packet counts are not unique application
goodput. Existing [aggregate counter limitations](experiments.md) still apply.
Zero-duration initial samples have null rates rather than invented zeros.

## Statistics and retention

Mean and sample standard deviation use a Welford recurrence with long-double
accumulators over all observed DATA deliveries in the collector scope. Minimum,
maximum, count and fixed histogram counts also cover all observations. Histogram
edges are 1, 5, 10, 50, 100, 500 and 1000 ms; bins are [lower, upper), followed by
an unbounded final bin. Histogram counts sum to the complete observed count.

Median/p95/p99 sort retained deliveries and interpolate at `p*(n-1)`. They cover all
observations only while retention is sufficient. Otherwise `percentile_scope`
explicitly identifies the most recent retained deliveries, with retained/discarded
counts. This is deterministic recent-window retention, not an unbiased reservoir,
a quantile confidence bound or an SLA guarantee. No observations gives null
statistics; a single observation gives null sample standard deviation.

| Limit | Default | Maximum |
| --- | ---: | ---: |
| Retained time samples (`max_samples`) | 2048 | 10000 |
| Global retained latencies | 4096 | 100000 |
| Flow groups and destination groups, independently | 128 | 1024 each |
| Retained latencies per group | 128 | 1024 |
| Pending serialization records | 65536 | 1000000 |

Time series and observed configuration-change logs retain their newest entries;
discarded counts are explicit. Window latency retains at most 256 recent entries
while its moments/histogram count every delivery in that window. New groups beyond
the group cap increment ungrouped-delivery counters; existing groups continue.
Global statistics continue across group overflow. Retained link samples are also
limited to one million. Limits count records, not an exact allocator-byte promise.

## Link utilization and queues

The engine records each admitted hop's serialization interval, including a modeled
probabilistic loss that consumes transmitter time. For each sampling window, the
collector clips those intervals to the window and divides summed busy seconds by
window duration. Propagation is excluded. Full duplex exports independent `ab`
and `ba` fractions; half duplex exports one `shared` fraction; simplex exports
`ab` only. Disabled or undefined slots are null. The native invariant rejects
utilization beyond one, apart from tiny floating-point rounding.

Initial topology links use stable original array indices. Removed links become
unavailable; newly added links are not registered and increment untracked-send
counts. Windows spanning incompatible duplex modes have undefined utilization.
If serialization retention overflows, affected links' subsequent utilization is
unknown, not a falsely low estimate. Summaries distinguish valid observed windows
and coverage; a maximum with no valid window is null. Already accepted reservations
keep timing through parameter changes under the existing link model.

Queue fractions use the busiest independent directional queue, or the shared
queue, divided by its configured capacity. Entries currently persist through
propagation until arrival. This is not a physical waiting-buffer measurement.
`link_changes` logs configuration changes seen at sample times, including faults;
flapping between samples can be missed. Fault Lab's own event log remains the
source of exact planned-fault timestamps.

## Deterministic diagnostic rules

Recommendations identify a rule, evidence, thresholds/time window, suggested
experiment and limitations. They are heuristics, not causal diagnoses:

- Directional serialization fraction >= 0.9 in a complete observed window:
  investigate demand/bandwidth alternatives. A brief peak need not be sustained.
- Sampled directional/shared queue fraction >= 0.8: compare propagation, queue
  policy and capacity before changing buffers.
- Observed path drops > 0: inspect availability, configured loss and drop counters;
  aggregate drops do not establish an abnormal probability or identify a flow.

The existing topology-only `NetworkAnalyzer` remains available for structural
bridges, articulation points and route concentration; its findings are distinct
from these measured runtime indicators.

## Verification and remaining work

Native tests check known moments/percentiles, large-offset numerical stability,
serialization versus propagation, full/half duplex, overflow/retention honesty,
sampled faults, modeled-loss serialization and deterministic TCP equivalence.
Python integration checks actual CLI collection, config/report validation,
byte-identical replay and unchanged aggregate CSV for a horizon within the run.

This increment does not model device processing rates, unique goodput, per-flow
path drops, deadlines, jitter, automatic traffic imbalance diagnoses, streaming
disk storage or a Studio Analytics Dashboard. Compare still consumes aggregate
CSV metrics; retained-scope percentile reports are not silently mixed into its
full-run summaries. Collection uses one owner thread and may add runtime/memory
overhead. Cross-platform floating-point bit identity is not guaranteed.
