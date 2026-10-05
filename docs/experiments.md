# Experiments and Measurement Notes

## Status

This document records qualitative observations from early KNS experiments and
defines how to interpret current output. The original runs did not preserve all
topology, seed, build, and sample details, so their observations are informative
rather than a reproducible benchmark dataset.

For new work, record the full command, commit, topology file, environment,
number of repetitions, and raw CSV alongside any plot.

## Current measurements

`SimulationEngine::exportStatsCSV()` writes one aggregate row with:

| Field | Meaning |
| --- | --- |
| `packets_sent` | Packets accepted for transmission by a link. |
| `packets_delivered` | Packets that reached their final destination. |
| `packets_lost` | Packets rejected or dropped by the modeled network path. |
| `total_latency` | Sum of delivered DATA latency in simulated seconds. |
| `avg_latency` | `total_latency / data_packets_delivered`, or zero when no DATA arrived. |
| `packets_in_transit` | Packets still recorded in flight at export time. |
| `total_sessions` | TCP sessions retained by the engine. |
| `data_packets_delivered` | Delivered DATA packets used as the latency denominator. |
| `schema_version` | Aggregate CSV contract version; currently `1`. |
| `simulation_duration_s` | Elapsed simulation time at export, in seconds. |
| `seed` | Configured simulation random seed. |

Average latency includes DATA only. The runner derives delivery and loss rates
by dividing their counters by `packets_sent` (zero when no packets were sent).
Because sent counts link transmissions and delivered counts final arrivals,
these ratios are aggregate counter ratios, not end-to-end delivery probabilities
for a multihop workload.

Network throughput is `packets_delivered / simulation_duration_s`, including
control packets; it is undefined when simulated duration is zero. Host execution
time is reported separately as `wall_clock_duration_s` and never used as the
network-throughput denominator.

## Running a controlled comparison

Use the headless CLI directly:

```bash
./build/app/KNS \
  --headless \
  --topology app/topologies/mesh4.json \
  --routing-metric delay \
  --output results/mesh4-delay.csv
```

On a Visual Studio build, the executable is normally
`build/app/Debug/KNS.exe`. Routing metric values are `delay`, `bandwidth`,
`hop-count`, and `delay-bandwidth`.

[`scripts/run.py`](../scripts/run.py) consumes version 1 of this schema and
rejects missing, malformed, unsupported, or nonfinite statistics. Its
`--timeout` applies from each child's launch, including while waiting for a free
parallel slot. Any failed run produces a nonzero batch exit status while
preserving diagnostic JSON and CSV reports. Optional plotting failures are
reported as warnings and do not suppress those artifacts.

## Observation 1: loss probability and measured latency

Earlier experiments increased per-link loss probability and observed a higher
loss rate. They also observed a lower average among packets that arrived. That
second result does not mean loss improves network latency: it is consistent with
survivorship bias. Packets on longer, multi-hop paths encounter more independent
drop opportunities, so the delivered sample becomes weighted toward shorter
paths as loss rises.

A stronger experiment should report at least:

- offered packets, delivered packets, and loss rate;
- latency distribution for delivered DATA, not only a mean;
- path length or source/destination pair;
- repeated seeds with confidence intervals.

## Observation 2: packet size and serialization delay

Earlier runs used packet sizes of 1,500, 3,000, 4,500, and 6,000 bytes and
observed increasing latency. This matches the link serialization term:

```text
serialization_seconds = packet_size_bytes × 8 / (bandwidth_mbps × 1,000,000)
```

The expected relationship is linear only while route, queueing, loss,
retransmission, and protocol behavior remain comparable. A reproducible test
should hold topology, routing metric, offered load, loss, seed, and link mode
constant and preserve the raw rows for every packet-size condition.

## Reproducibility checklist

Use `--seed 42 --packet-size 1500` to make those parameters explicit. Repeat the
same command and topology with the same build when comparing results; a new
run reseeds the generator. Configure `queue_capacity` in each JSON link when
studying buffer size. Queue-management experiments must also record
`queue_policy` and, for RED, the min/max thresholds and maximum early-drop
probability. The current RED implementation uses instantaneous directional queue
occupancy rather than EWMA occupancy. A recovered loss is valid if the TCP workload completes
and no pending events, packets, buffers, or link queue entries remain.

- record the Git commit and build type;
- keep the topology JSON with the results;
- record the exact CLI and environment variables;
- use the same routing metric across a comparison unless it is the independent
  variable;
- preserve raw CSV before computing derived metrics;
- distinguish simulated seconds from wall-clock runtime;
- report failed headless validation instead of silently discarding the run;
- avoid causal conclusions from delivered-only samples under packet loss.


## Official routing baseline v1

The repository now includes a versioned benchmark definition at
`benchmarks/v1/routing-baseline.json`. It compares all four supported routing
metrics across `mesh4`, `mesh5`, and `star` using seeds 42, 43, and 44 with
a 1500-byte application payload.

Validate the matrix without running simulations:

```bash
python scripts/benchmark_suite.py --dry-run
```

After building KNS, execute the complete suite:

```bash
python scripts/benchmark_suite.py --timeout 60
```

Use `--exe <path>` when the executable cannot be discovered automatically and
`--output <directory>` to choose a deterministic artifact location.

Each benchmark run stores:

- the exact expanded case matrix;
- the KNS Git commit and executable path;
- platform and timeout metadata;
- the exact command for every case;
- per-case stdout/stderr logs and raw engine CSV;
- `metrics.csv` with comparable derived metrics;
- `benchmark.json` with all configuration and results.

The suite definition is version-controlled separately from its result artifacts.
Changing topologies, seeds, packet sizes, metrics, or interpretation rules should
produce a new benchmark suite version rather than silently changing
`routing-baseline-v1`.


The repository CI validates the suite definition and expanded matrix with
`--dry-run`, but it intentionally does not execute all 36 simulations on every
commit. Full benchmark result artifacts should be produced from a known-good KNS
build after the canonical build/test CI passes.


## Resilience baseline v1

`benchmarks/v1/resilience-baseline.json` compares normal operation against a
temporary deterministic link outage. The headless CLI accepts repeatable events:

```bash
./build/app/KNS --headless \
  --topology app/topologies/mesh4.json \
  --link-event 0.5:0:1:down \
  --link-event 2.5:0:1:up
```

Each event uses simulated time and is scheduled before workload generation, so
the same topology, seed, build and event list produce the same ordering. The
benchmark suite records the named fault scenario in `benchmark.json` and
`metrics.csv`. Existing suites omit `link_event_scenarios` and therefore run
as a single `baseline` scenario with no fault events.

The first official resilience baseline uses `mesh4`, Reno, delay routing and
seeds 42–44. It compares a control run with a link 0–1 outage from t=0.5 to
t=2.5. This benchmark measures simulator behavior under a controlled transient
failure; it is not a claim about real-world physical failure rates.


### Fault scenario comparison output

Suites with more than one `link_event_scenarios` entry also emit
`scenario-comparison.json` and `scenario-comparison.csv`. Each non-baseline
case is paired only with the baseline case that has the same topology, routing
metric, seed, packet size and congestion-control algorithm.

All reported deltas use **fault scenario minus baseline**:

- negative `delivery_rate_delta` means the failure reduced delivery rate;
- positive `loss_rate_delta` means the failure increased loss;
- negative `throughput_pps_delta` means the failure reduced logical throughput;
- positive `avg_latency_s_delta` means the failure increased delivered-packet latency;
- positive `simulation_duration_s_delta` means the workload took longer in simulated time.

Failed cases are not paired into a numeric comparison; their process/status
evidence remains available in `benchmark.json`.


### Human-readable benchmark report

Every official benchmark execution writes `report.md` next to
`benchmark.json` and `metrics.csv`. The report is derived from the same
recorded case data and includes the KNS version/commit, aggregate successful-case
metrics, per-case results, fault-vs-baseline deltas when applicable, and explicit
failed-case evidence.

The official GitHub Actions workflows append this Markdown report to the job
summary while still uploading the full machine-readable artifact directory.


## AQM baseline v1

`benchmarks/v1/aqm-baseline.json` is the first controlled queue-management
comparison. It uses two four-node line topologies with the same nodes, links,
delay, bandwidth, loss, queue capacities and routing assumptions. The central
link is a 1 Mbps bottleneck with capacity eight.

The control topology uses drop-tail. The paired RED topology changes only the
bottleneck queue configuration:

```text
min threshold = 2 packets
max threshold = 6 packets
max early-drop probability = 0.25
```

Both variants run Reno, delay routing, 1500-byte payloads and seeds 42–46. This
produces ten cases. The paired topology files are protected by a regression test
that rejects accidental non-AQM differences between them.

Run the suite locally with:

```bash
python scripts/benchmark_suite.py \
  --suite benchmarks/v1/aqm-baseline.json \
  --exe build/app/KNS \
  --timeout 60
```

The scheduled/manual **AQM benchmark v1** workflow builds a clean Release binary,
executes the same matrix, appends `report.md` to the Actions summary and retains
the full result directory as an artifact.

This baseline characterizes the implemented simulator model; it is not a claim
that RED universally outperforms drop-tail. In particular, KNS RED v1 uses
instantaneous directional queue occupancy rather than the EWMA average used by
many classic RED formulations.


## AQM drop attribution

Stats CSV schema version 1 now carries two additive counters:

- `queue_overflow_drops`: packets rejected because the selected link queue had
  no remaining admission capacity;
- `red_early_drops`: packets rejected by RED before queue admission.

Both counters are subsets of `packets_lost`; their sum must never exceed the
total loss count. Other loss causes such as configured random link loss, invalid
forwarding state, device-role rejection, or topology failure remain represented
only in `packets_lost`.

These fields are additive to CSV schema version 1. Historical v1 CSV files that
do not contain them are interpreted by the runner as zero. This preserves old
experiment readability while allowing AQM experiments to distinguish queue
overflow from RED's intended early-drop behavior.
