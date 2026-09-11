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
| `avg_latency` | `total_latency / packets_delivered`, or zero when none arrived. |
| `packets_in_transit` | Packets still recorded in flight at export time. |
| `total_sessions` | TCP sessions retained by the engine. |

The average-latency denominator currently counts every delivered packet, while
`total_latency` accumulates DATA packets only. Treat this field as the engine's
current aggregate statistic rather than a pure DATA-only mean.

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

[`scripts/run.py`](../scripts/run.py) is a historical automation runner. Its CSV
parser expects a legacy column schema and must be aligned with the current
exporter before its derived reports are treated as authoritative.

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

- record the Git commit and build type;
- keep the topology JSON with the results;
- record the exact CLI and environment variables;
- use the same routing metric across a comparison unless it is the independent
  variable;
- preserve raw CSV before computing derived metrics;
- distinguish simulated seconds from wall-clock runtime;
- report failed headless validation instead of silently discarding the run;
- avoid causal conclusions from delivered-only samples under packet loss.
