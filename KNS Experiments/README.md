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
| `analytics` | Optional shared native collection config; see [Analytics](../docs/analytics.md). |
| `traffic_plan` | Optional embedded abstract workload; see [Traffic Lab](../docs/traffic-models.md). Packet wire sizes come from this plan. |
| `validation` | Optional Traffic Lab assertions applied to every case. Relative rules use the profile's `baseline`. Requires `traffic_plan`. |

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

`analytics-lab.json` adds native time series, retained-scope DATA latency statistics
and heuristic recommendations. Config is persisted in the manifest and each case's
`.csv.analytics.json` report is required/validated when requested. The raw aggregate
CSV retains its existing meanings; Analytics observation horizons and percentile
retention scopes are explicit and can differ from the full simulation duration.
Use `python scripts/analytics_report.py <case>.csv.analytics.json --output <new-directory>`
for local charts, CSV and diagnostic evidence. Complete-report validation checks
statistical conservation, coverage, rates and within-flow latency-change counts.
The [Traffic Lab example](../docs/traffic-models.md) offers six concurrent schedules
on a shared backbone through `--traffic-plan`.

For profiles that include a `traffic_plan`, add a top-level `validation` object
to check every case against the same rules. The example below requires at least
90% delivery across all flows.

```json
{
  "validation": {
    "schema_version": "1.0",
    "assertions": [
      {"name": "minimum-delivery", "metric": "delivery_ratio", "operator": ">=", "value": 0.9}
    ]
  }
}
```

During preflight, the runner validates the rules and checks that any referenced
`flow_id` exists in the plan. The manifest retains the rules for replay, and
`--dry-run` shows them with the case matrix. Execution writes
`validation/rules.json`, `validation/summary.json`, a combined JUnit
`validation/summary.xml` and per-case JSON, JUnit,
Markdown and HTML verdicts under `validation/<case-id>/`. When a case has no
complete report, the summary records the error alongside its benchmark status.

Profile execution exits with code 0 when the benchmark and all assertions pass,
1 on a benchmark or validation execution error, and 2 when execution succeeds
and at least one assertion fails. The manifest links to the validation summary.
The experiment directory retains the benchmark and comparison artifacts.
Rules 1.1 add `baseline_offset`, and rules 1.2 add `baseline_percent`.
These select the profile's named `baseline` case with identical seed, routing
metric, packet-size and congestion-control settings. Baseline cases evaluate
against themselves, including every absolute and relative assertion. A strict
improvement rule can therefore fail on the baseline itself. The same rule set
applies to every variant.

Relative validation requires identical effective traffic plans across variants.
Preflight rejects workload-changing overrides or sweeps. During evaluation,
complete reports, workload hashes, models, recorded random streams and primary
generated counts must match, as in the
[standalone validator](../docs/validation.md). A missing, failed or incompatible
reference produces a validation error and exit code 1. The runner keeps each
reference case ID and report hash with its verdict. Reference selection is
independent of variant ordering and is preserved by replay.

The combined JUnit file groups assertions by case ID with distinct test
identities. Threshold violations become failures. Missing, incomplete or
incompatible evidence becomes an error test for the affected case, including
cases whose baseline is unavailable. Root and per-suite counters cover every
emitted test. The HTML overview links to the combined JUnit file, and saved-result
analysis produces the same artifact.

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
- `report.html`: offline overview produced after execution, with the experiment
  status, per-case execution and validation outcomes, comparison links, input
  hashes and links to the recorded evidence. The manifest's `report` field
  identifies this page. Links are relative to the bundle, so the overview works
  after moving the entire directory. Requested validation reports appear under
  `validation/`. Failed cases keep their errors visible in the overview.

Open `report.html` in a browser after the profile command finishes. The overview
uses inline styles and links to evidence within the bundle. Keep the directory
together when sharing the results. A `--generate-only` bundle contains prepared
inputs and a manifest, and receives its execution report when replayed with KNS.

## Analyze a saved experiment

Regenerate comparison, validation and overview reports from a finished bundle
with the current Python tools:

```powershell
python scripts/analyze_experiment.py results/traffic-experiment --output "results/traffic analysis"
```

Pass `--rules` to evaluate a different set of assertions against the saved cases.
The command below applies the standalone delivery target to every case.

```powershell
python scripts/analyze_experiment.py results/traffic-experiment --rules "KNS Experiments/traffic-lab/assertions.json" --output "results/new delivery criteria"
```

The input rules file is bounded to 1 MiB and goes through profile preflight,
including flow IDs, baseline compatibility and effective workload checks. The
analysis stores the applied rules in `validation/rules.json`. Its provenance
records `validation_rules_sha256` for that stored file and
`validation_rules_source_sha256` for the supplied file's original bytes.
The source configuration identity and recorded execution outcome remain
available alongside the new analysis outcome. A run that violated its original
assertions can pass a new set of criteria when all recorded cases executed
successfully. Invalid rules fail before the report directory is created.

Use `--baseline <variant-name>` to select a different reference for comparisons
and baseline-relative rules in the new analysis. The variant must exist in the
saved profile. The source manifest retains its original reference choice.
`analysis.json` records the applied `comparison_baseline` and whether it was
explicitly selected. The comparison section displays the applied reference.

The destination must be new. It receives `report.html`, comparison reports,
per-case validation artifacts when rules are embedded, and `analysis.json`.
Analysis provenance records the source run ID, configuration identity, source
manifest/benchmark hashes and the current analyzer, comparator, validator and
overview-exporter hashes. The overview shows both the recorded execution status
and the current analysis status.

`analysis.json` follows the
[experiment-analysis v1 contract](../contracts/experiment-analysis-v1.schema.json).
The contract defines completed analysis provenance, assertion-failure outcomes
and errors recorded after report-directory creation. The contract validator
can check an emitted result alongside the fixtures.

```powershell
python scripts/validate_contracts.py --experiment-analysis "results/traffic analysis/analysis.json"
```

The command reads case evidence from the bundle's `runs/` directory, including
bundles moved since execution or recorded on another platform. Traffic report
configurations must match the saved profile's effective per-variant plan.
Missing evidence, incomplete matrices and incompatible baseline reports prevent
a passing analysis. Prepared and running manifests are rejected.

New benchmark cases record `artifact_sha256` for their CSV, log and any Traffic,
Analytics or Fault JSON reports. Saved-result analysis verifies every recorded
digest before creating its output directory. Missing files, malformed/incomplete
digest maps and byte changes are input errors. File locations are resolved within
the bundle's `runs/` directory, preserving analysis after moving the bundle or
reading metadata from another platform.

`analysis.json` records `evidence_integrity` with counts of hashed cases, legacy
cases and verified files. The HTML overview displays this coverage. Older bundles
without `artifact_sha256` retain their existing analysis behavior and appear as
legacy cases. Digests identify file bytes relative to the recorded benchmark
metadata.

Analysis also reconciles cached benchmark values with the engine CSV using the
runner's existing parser and metric calculations. This covers seed, packet/drop
counters, delivery/loss ratios, latency, throughput and virtual duration. A
disagreement is an input error detected before output creation. Live benchmark
execution rejects a statistics CSV whose seed differs from the configured case.

The analysis provenance records `metric_consistency` with counts of reconciled
cases and cases with unavailable statistics. Legacy bundles can retain absent
or null cached fields, and cases without a recorded CSV appear as unavailable.
Cases declaring artifact hashes require a statistics CSV after successful
execution. The overview shows this coverage alongside file-integrity coverage.

Analytics and Fault Lab JSON reports are also checked against the configuration
embedded in the saved profile. Analysis reuses the live runner's validators for
observation completeness, statistical consistency and applied fault events.
Missing required reports, unexpected report declarations and inconsistent
content are input errors detected before report-directory creation. These
checks also apply to legacy bundles. Auxiliary JSON inputs use the existing
64 MiB report limit.

Traffic Lab reports are validated in profiles with and without embedded
assertions. Their validated-report count is included in `auxiliary_reports`.

`analysis.json` records `auxiliary_reports` with separate counts of validated
Analytics, Fault Lab and Traffic Lab reports and cases with unavailable execution evidence.
The overview displays those counts. Cases whose execution failed retain their
failure status and diagnostic evidence.

The original bundle is preserved. Overview links to its raw evidence are relative
to the new analysis directory. Keep both directories in the same relative layout
when sharing them. Exit codes are 0 when the recorded execution and current
assertions pass, 1 for input/execution/analysis errors, and 2 for violated
assertions after successful execution. Current validators may reject evidence
accepted by an older tool version, so retain `analysis.json` with the reports.

## Compare variants

The [Compare guide](../docs/compare.md) covers offline HTML/SVG output, side values,
percentages, seed variability, conditional confidence intervals and comparison of
independent topology bundles using `--against`. Comparison output is version 1.1;
the raw simulator CSV contract is unchanged. New outputs include `comparison.html`
and `comparison-summary.csv` alongside the existing JSON, paired CSV and Markdown.

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
Comparison input is limited to 1 MiB for each profile and 64 MiB for each benchmark.
Duplicate fields and non-finite JSON numbers are rejected before reports are created,
including when comparing two separate bundles with `--against`.
It regenerates comparison reports and preserves the profile, raw results
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
