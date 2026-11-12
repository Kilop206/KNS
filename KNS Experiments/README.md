# KNS Experiments

Create controlled variants of one topology link and compare them using the
existing headless benchmark runner. Python 3 is the only runner dependency;
executing simulations also requires a built KNS binary.

From the KNS repository root, validate the bundled profile without generating
files or launching the simulator:

```powershell
python scripts/experiment_profile.py --dry-run
```

Generate topology variants and execute the three bandwidth scenarios:

```powershell
python scripts/experiment_profile.py --profile "KNS Experiments/experiment-profile.json" --exe build/app/KNS.exe
```

Use the executable path for your build, such as `build/app/Release/KNS.exe` with
Visual Studio. Omit `--exe` to search `build/` automatically. Pass `--timeout 60`
to set the wall-clock limit for each case (60 seconds by default).

Use `--generate-only` to prepare variants without a binary. `--output <directory>`
selects a **new** artifact directory; existing directories are rejected.
Without it, results go under `KNS Experiments/results/<profile-name>/<UTC-time>/`.

On systems with Bash, the existing editor also dispatches profile execution:

```bash
bash "KNS Experiments/experiments.sh" --profile "KNS Experiments/experiment-profile.json" --dry-run
```

The `--profile` option must come first in this Bash command. Python is the direct
entry point on Windows. Profile execution does not require `jq`.

## Profile fields

The bundled `experiment-profile.json` compares 100, 500 and 25 Mbps on link index
zero of `services-lab.json`. Every variant starts from the original topology.
Only explicitly specified fields on the selected link change; device services,
other links, queue policies and metadata are retained.

| Field | Meaning / default |
| --- | --- |
| `schema_version` | `"1.0"`; omitted legacy versions normalize to this version. Other versions are rejected. |
| `max_cases` | Complete matrix budget, default `1000`, configurable from `1` to `10000`. Checked before expansion/output. |
| `name` | Profile name; letters, digits, hyphens and underscores. |
| `topology` | Input JSON path, relative to the **KNS repository root**, or absolute. |
| `index` | Zero-based link array index; defaults to `0`. |
| `experiments` | Non-empty list of named variants. Names must be unique ignoring case. |
| `baseline` | Reference experiment name (exact spelling); defaults to the first variant. |
| `bandwidth` | Per-variant link bandwidth in Mbps, greater than zero. |
| `delay` | Per-variant link propagation delay in milliseconds, at least zero. |
| `loss` | Per-variant loss probability, between zero and one. |
| `queue_capacity` | Positive integer capacity (at most 2147483647); may introduce the optional topology field. |
| `sweep` | Parameter lists or `{start, stop, step}` intervals; mutually exclusive with `experiments`. |
| `seeds` | Distinct unsigned 64-bit seeds; defaults to `[42]`. |
| `packet_sizes` | Positive payload sizes in bytes; defaults to `[1500]`. |
| `routing_metrics` | Defaults to `["delay"]`; also supports `bandwidth`, `hop-count`, `delay-bandwidth`. |
| `congestion_controls` | Defaults to `["reno"]`; also supports `tahoe`, `newreno`, `cubic`. |
| `allow_transport_failures` | Accept orderly retry exhaustion as an experiment outcome; defaults to `false`. |

Each experiment requires `name` and at least one of `bandwidth`, `delay`, `loss`,
`queue_capacity`.
Dimensions expand as a Cartesian product: three variants with three seeds and
two payload sizes produce eighteen cases. Use distinct seeds for repetitions.
Relative `--profile`, `--output` and `--exe` arguments resolve against the caller's
working directory. Topology paths inside profiles always resolve against KNS.

## Bounded sweeps and replay

[The v1 JSON Schema](../contracts/experiment-profile-v1.schema.json) describes the
profile shape; runtime validation also checks baseline references, link indices,
parameter domains, interval bounds, case-insensitive variant names and the total
matrix budget. No schema library is needed to run experiments. The existing
contract validator checks the schema and its positive/negative fixtures in CI.

`capacity-sweep.json` explores bandwidth, propagation and queue capacity over two
seeds (24 runs). To evaluate which changes affect congestion, run it and inspect
the paired differences; combinations changing several parameters do not isolate
causality. This sweep is not a certified maximum-capacity estimator.

```powershell
python scripts/experiment_profile.py --profile "KNS Experiments/capacity-sweep.json" --dry-run
python scripts/experiment_profile.py --profile "KNS Experiments/capacity-sweep.json" --exe build-core-mingw/app/KNS.exe --output results/capacity
python scripts/experiment_profile.py --replay results/capacity/manifest.json --exe build-core-mingw/app/KNS.exe --output "results/capacity replay"
```

Sweeps accept discrete lists and ascending inclusive-grid intervals (positive
step, stop >= start). Decimal arithmetic avoids binary increment accumulation;
the stop is included only when it lies on the grid. Keys expand in sorted order,
with names `sweep-0001`, etc.; the first variant is the default reference. Saved
profiles contain explicit expanded variants so replay need not expand a range
again. Seed, payload, routing and TCP algorithm dimensions multiply that count.
Unsupported traffic/device sweep dimensions are rejected.

Every generated bundle includes `manifest.json`, even in `--generate-only` mode.
It embeds the normalized profile, original topology and per-case wall timeout,
plus a SHA-256 of canonical UTF-8 JSON (sorted keys, compact separators, finite
numbers). Original input paths and artifact paths are excluded from the canonical
configuration, so moving a bundle preserves its hash. Timestamps, run UUIDs and
execution durations are provenance, not deterministic simulation outputs.

The manifest records source revision/version, binary SHA-256, runner source-file
hashes and Python/OS/machine details. Source version is read from CMake and is not
a claim about the identity of an arbitrary executable; the binary hash is the
identity check. No environment secrets are collected. Each benchmark case also
records a unique run UUID, UTC start/end, status, command and virtual/real duration.
Metrics and errors remain in `runs/benchmark.json` and raw CSV/logs.

Replay validates the manifest's version and configuration hash and reconstructs
topologies from embedded inputs. It works after moving artifacts or deleting the
original profile/topology. It creates a new output directory and run ID and records
`replay_of`. A binary differing from the recorded hash is rejected unless
`--allow-executable-change` is explicit; that change is recorded. A prepared
bundle without a binary hash has no previous binary identity to enforce.
`--timeout` overrides the saved timeout and changes the configuration hash.

The status progresses `prepared` -> `running` -> `completed`/`failed`. Abnormal
process termination can leave `running`, which is not a success. Completion means
the configured outcome policy passed; accepted transport failures remain visible
in case status/counts. Hashes detect input edits, not malicious re-signing, and do
not imply authentication. Equivalent environment/model conditions are still
required; cross-platform floating-point identity is not guaranteed. This is
configuration replay from the beginning, not restoration of engine state.

## Artifacts and failures

Named variants can also embed a `fault_plan` for link outages, parameter degradation
or flapping. The [Fault Lab guide](../docs/fault-injection.md) describes the
`fault-lab.json` example, in-flight policy, observations and recovery limitations.
These plans generate `fault-plans/` inputs and per-case `.csv.faults.json` reports;
the manifest embeds them for portable replay. Missing/incomplete execution
evidence fails the case even if aggregate statistics exist.

Each output directory contains:

- `profile.json`: validated profile with explicit defaults;
- `source-topology.json`: snapshot of the unmodified input;
- `topologies/`: one generated topology per named variant;
- `suite.json`: generated benchmark definition with absolute topology paths;
- `runs/benchmark.json`, `runs/metrics.csv`, `runs/report.md`: execution metadata,
  per-case metrics and a readable comparison table;
- `runs/*.csv` and `runs/*.log`: raw simulator statistics and combined process logs.
- `comparison.json`, `comparison.csv`, `comparison.md`: paired differences from
  the reference experiment, with summaries across seeds and unavailable-pair evidence.

## Compare variants

Completed profile executions automatically compare each non-reference variant
against `baseline` (the first experiment by default). Each pair uses the same seed,
routing metric, payload size and congestion control. Every delta is **experiment
minus baseline**. Positive latency deltas mean increased delivered-packet latency;
negative throughput deltas mean lower logical throughput. Delivery/loss deltas
are differences between aggregate counter ratios.

Summaries average paired deltas across seeds separately for each variant, routing
metric, payload size and TCP algorithm. They include contributing sample counts
per metric; JSON also preserves minimum and maximum deltas. Undefined metrics are
left empty in CSV and shown as N/A in Markdown. Failed, timed-out, transport-failed
and missing cases remain explicitly unavailable and never enter numeric averages,
including when the profile allows transport failures as execution outcomes.

Rebuild the comparison from an existing artifact folder without running KNS:

```powershell
python scripts/compare_experiments.py "KNS Experiments/results/development-validation"
```

Choose another reference without changing the recorded profile:

```powershell
python scripts/compare_experiments.py "KNS Experiments/results/development-validation" --baseline high-bandwidth
```

Analysis reads only `profile.json` and `runs/benchmark.json`; it also supports
older profile artifacts without an explicit baseline, and moved artifact folders.
It regenerates the three comparison files and preserves the profile, raw results
and logs. Successful analysis returns zero even if some pairs are unavailable;
the original profile execution still returns the benchmark batch's failure code.

Generated suites contain absolute paths to this artifact directory. To execute
a prepared suite later on the same machine, choose a new run directory:

```powershell
python scripts/benchmark_suite.py --suite "path/to/artifacts/suite.json" --output "path/to/new-runs" --exe build/app/KNS.exe
```

A failed process, timeout, startup error or invalid statistics makes the batch
exit nonzero while preserving reports and continuing the remaining cases.
Only explicit `allow_transport_failures: true` accepts transport retry exhaustion;
the outcome is still recorded as `transport_failure`.

Throughput uses simulated seconds. Host runtime is recorded separately. Delivery
and loss rates are aggregate counter ratios; see
[measurement definitions](../docs/experiments.md). A bandwidth variant is a
controlled simulator experiment, not a guarantee about real network performance.

## Single-link editor

The original Bash interface remains available and requires `jq`:

```bash
bash "KNS Experiments/experiments.sh" -t app/topologies/services-lab.json -i 0 -bw 25 -d 10 -l 0.01 -o "KNS Experiments/results/variant.json"
```

It writes one topology without launching KNS. Delay is in milliseconds and loss
is a probability. The input file must differ from the output file.
