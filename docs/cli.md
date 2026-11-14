# CLI reference for implemented commands

The native executable currently shares the Studio build. Python entry points
provide experiment orchestration, comparisons, validation and trace conversion.
A unified `kns simulate` / `kns experiment` subcommand interface remains pending;
the commands below are the working interface. See [testing.md](testing.md) for
build paths and guards, and each entry point's `--help` for all arguments.

## Native simulation

Windows examples use `build/app/KNS.exe`; Linux uses `build/app/KNS`. Replace the
path with your configured target, e.g. `build-core-mingw/verification-build/app/KNS.exe`.
Quote paths containing spaces.

```powershell
build/app/KNS.exe --headless --topology app/topologies/mesh4.json --seed 42 --output results/simulation.csv
build/app/KNS.exe --headless --topology app/topologies/mesh4.json --until 0.5 --max-events 10000 --output results/bounded.csv
```

`--until` is an inclusive virtual horizon in seconds; `--max-events` counts engine
events rather than packets. Remaining work produces an incomplete snapshot and
exit 3. The CSV remains available for inspection. An incomplete snapshot is not
a completed network result. Packet size, routing metric and congestion control
are configurable through `--packet-size`, `--routing-metric` and
`--congestion-control`. Explicit `--traffic-plan`, `--fault-plan` and `--analytics`
JSON inputs select the documented workload/fault/observation models.

An explicit Traffic Lab plan disables automatic TCP workloads. Its offered and
delivered wire-byte ledger must not be interpreted as TCP application goodput.
Native exit codes are 0 for completed accepted execution, 1 for invalid input or
execution error, 2 for failed automatic-TCP transport validation, and 3 for an
incomplete bounded execution. Full validation rules are model-dependent.

## Experiments, sweeps and replay

```powershell
python scripts/experiment_profile.py --profile "KNS Experiments/traffic-lab/profile.json" --exe build/app/KNS.exe --output results/traffic
python scripts/experiment_profile.py --replay results/traffic/manifest.json --exe build/app/KNS.exe --output results/traffic-replay
```

A profile may contain bounded parameter sweeps; no separate sweep flag is needed.
`--dry-run` validates/expands without writing files; `--generate-only` writes the
configuration without running KNS. `--timeout` is real seconds per case and is
recorded in the manifest. Replay enforces the pinned executable identity unless
`--allow-executable-change` is deliberately supplied and recorded. Destinations
must be new. Failed cases retain diagnostics; their metrics are not successful
simulation evidence. See [experiments.md](experiments.md).

```powershell
python scripts/compare_experiments.py results/reference --against results/candidate --output results/comparison
```

Cross-bundle comparison pairs compatible workloads/seeds; `--baseline`,
`--candidate`, `--reference-routing` and `--candidate-routing` choose variants.
Differences do not by themselves establish causality or statistical confidence.

## Assertions and reports

```powershell
python scripts/validate_result.py results/traffic/runs/003-002-upgrade-delay-reno-baseline-s42-p1500.csv.traffic.json --rules "KNS Experiments/traffic-lab/assertions.json" --output results/validation
python scripts/analytics_report.py results/simulation.csv.analytics.json --output results/analytics-report
```

The assertion example consumes the completed Traffic Lab example above. The
analytics command requires a native Analytics JSON report from an observation-
enabled run; replace its input with that report. Neither command reruns the
simulation. Validation writes JSON, JUnit XML and Markdown: exit 0 means all rules
passed, 1 means valid input with failed assertions, and 2 means invalid input.
Reports preserve metric scopes, units, denominators and model limitations.
Rules 1.1 accept `baseline_offset` and a complete `--baseline` report with the
identical workload, recorded streams and source cohorts; see [validation.md](validation.md).

## Trace import and industrial/capacity scenarios

```powershell
python scripts/trace_import.py "KNS Experiments/trace-lab/authored-ethernet.pcapng" --topology "KNS Experiments/trace-lab/topology.json" --address-map "KNS Experiments/trace-lab/address-map.json" --output results/pcapng --exe build/app/KNS.exe
python scripts/industrial_scenario.py --sensors 100 --redundant --timeout 180 --output results/industrial --exe build/app/KNS.exe
python scripts/capacity_planner.py --base-sensors 10 --growth-percent 0 100 --bandwidth-mbps 2 4 --exe build/app/KNS.exe --output results/capacity
```

For source-only JSON analysis, run the same trace command with `--analyze` in
place of `--output ... --exe ...`. This does not require a native executable,
write an experiment or execute the simulator. The output is explicitly scoped
to source record metadata and structural topology preflight. Runtime input
errors are JSON on stderr (exit 1); argparse usage errors exit 2. The analysis
result contract is [trace-analysis-v1](../contracts/trace-analysis-v1.schema.json).

The trace is explicitly authored, not a real-network measurement. See
[trace-import.md](trace-import.md) for supported CSV/JSON/PCAP/PCAPNG subsets,
explicit address mapping, limits and privacy defaults. Omitting `--exe` generates
trace/industrial inputs only. Industrial timeout is real seconds per executed
case; sensor schedules still use virtual time. Capacity alternatives are actually
simulated and bounded by `--max-cases`; optional `--costs` uses user-supplied data.
See [iiot.md](iiot.md) for industrial assumptions and outstanding models.

Most Python orchestration commands return 0 on success and 1 on input/run/report
failure; argument-parser usage errors return 2. Validation has the explicit
0/1/2 contract above. Legacy diagnostics remain human-readable; consume the
versioned result/manifest/validation JSON artifacts for automation. General
structured errors, independently built native CLI and unified subcommands remain
pending. These commands perform isolated simulation/file processing and do not
apply real network emulation.
