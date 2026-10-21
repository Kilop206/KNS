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
| `name` | Profile name; letters, digits, hyphens and underscores. |
| `topology` | Input JSON path, relative to the **KNS repository root**, or absolute. |
| `index` | Zero-based link array index; defaults to `0`. |
| `experiments` | Non-empty list of named variants. Names must be unique ignoring case. |
| `bandwidth` | Per-variant link bandwidth in Mbps, greater than zero. |
| `delay` | Per-variant link propagation delay in milliseconds, at least zero. |
| `loss` | Per-variant loss probability, between zero and one. |
| `seeds` | Distinct unsigned 64-bit seeds; defaults to `[42]`. |
| `packet_sizes` | Positive payload sizes in bytes; defaults to `[1500]`. |
| `routing_metrics` | Defaults to `["delay"]`; also supports `bandwidth`, `hop-count`, `delay-bandwidth`. |
| `congestion_controls` | Defaults to `["reno"]`; also supports `tahoe`, `newreno`, `cubic`. |
| `allow_transport_failures` | Accept orderly retry exhaustion as an experiment outcome; defaults to `false`. |

Each experiment requires `name` and at least one of `bandwidth`, `delay`, `loss`.
Dimensions expand as a Cartesian product: three variants with three seeds and
two payload sizes produce eighteen cases. Use distinct seeds for repetitions.
Relative `--profile`, `--output` and `--exe` arguments resolve against the caller's
working directory. Topology paths inside profiles always resolve against KNS.

## Artifacts and failures

Each output directory contains:

- `profile.json`: validated profile with explicit defaults;
- `source-topology.json`: snapshot of the unmodified input;
- `topologies/`: one generated topology per named variant;
- `suite.json`: generated benchmark definition with absolute topology paths;
- `runs/benchmark.json`, `runs/metrics.csv`, `runs/report.md`: execution metadata,
  per-case metrics and a readable comparison table;
- `runs/*.csv` and `runs/*.log`: raw simulator statistics and combined process logs.

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
