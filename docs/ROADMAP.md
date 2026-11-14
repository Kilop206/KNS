# Network engineering platform roadmap

This roadmap concerns the simulator platform requested on 2026-11-12. The
separate `implementation/ROADMAP.md` tracks ecosystem integration and remains valid.
No milestone below is certified complete by the existence of an interface.

| Milestone | Dependencies | Acceptance evidence | Audited state |
| --- | --- | --- | --- |
| 1 Core reliability | baseline build/tests | deterministic replay, bounded execution, analytical link/queue tests, reconciled packet/byte metrics | Partial: clock, ordering, cancellation, seeded RNG, duplex/RED and TCP exist; execution control and metrics need expansion |
| 2 Experiments | 1 | versioned scenarios, bounded sweeps, replayable hashed manifests | Partial: versioned profiles, bounded list/range sweeps, queue variants, hashed portable replay, per-case identity/provenance implemented; full scenario contract pending |
| 3 Fault Lab | 1, 2 | scheduled failures, explicit in-flight policy, before/during/after results | Partial: versioned link plans, down/up/degradation/flapping, queue/in-flight policy, route observations and interval reports integrated with CLI/profiles/replay; node/device/service models pending |
| 4 Compare | 2, 3 | paired equivalent workloads, variability, JSON/CSV/HTML | Partial: cross-bundle topology comparisons, explicit routing pairs, side/relative deltas, seed variability, conditional t intervals and offline HTML/SVG added; unavailable engine metrics and Studio view pending |
| 5 Analytics | 1, 2 | stable statistics, retained time series, evidence-based rules | Partial: native collection, TCP payload-release goodput, within-flow latency variation, pressure/imbalance heuristics, semantic validation and offline HTML/CSV export; device processing, protocol-specific jitter, routing concentration and Studio pending |
| 6 Traffic Lab | 1, 2 | concurrent CBR/periodic/burst/Poisson and application models | Partial: six bounded schedules, per-flow seeds, causal application abstractions, operation/deadline accounting, joint link/traffic sweeps, CLI/schema/replay/examples; general dependency graphs, arbitrary distributions and Studio pending |
| 7 Industrial | 3, 5, 6 | deadline accounting, documented MQTT abstraction, scaling report | Partial: explicit telemetry sensors and PLC/actuator exchange, uplink outage/recovery/redundancy/congestion, actual 10/100/250-sensor measurements; MQTT, node/device failures, 1000/10000 scales and Studio pending |
| 8 Capacity | 2, 4, 5, 6 | growth sweeps and user-priced alternatives | Partial: bounded actual sensor-growth/bandwidth alternatives, explicit requirement verdicts, optional outage/redundancy, user-priced Pareto frontier and retained evidence; broader workloads/costs/search/Studio pending |
| 9 Validate | 2, 5 | assertions, baselines, diagnostic CI failures | Partial: explicit workload assertions, count/deadline/latency scopes, JSON/JUnit/Markdown, exit codes, actual regression test and deterministic CI reference; baseline-relative and scenario-embedded rules pending |
| 10 Trace import | 2, 6 | bounded malformed-input-safe CSV/JSON then PCAP replay | Partial: CSV/JSON and classic PCAP Ethernet/IPv4 TCP/UDP metadata import with up to two VLAN tags, explicit node mapping, ordered replay, size/interarrival profiles and label suppression; PCAPNG/IPv6/reassembly and broader capture adapters pending |
| 11 Emulation | isolated optional adapter | dry run, explicit consent, isolated Linux test and cleanup | Pending; never change host networking implicitly |
| 12 Studio | 1–9 | responsive functional editors and experiment/analysis views | Partial: existing ImGui studio retained |
| 13 CLI | 1–10 | independently buildable CLI, subcommands, structured failures | Partial: legacy headless options retained |
| 14 Reports | 2–9 | self-contained HTML, JSON, CSV, Markdown with methodology | Partial: CSV and experiment comparisons exist |
| 15 Extensions | stable model contracts | interchangeable internal strategies with conformance tests | Partial: congestion control strategies exist |
| 16 Performance | correctness baselines | measured event/packet throughput, memory, topology scaling | Partial: benchmark scripts exist |
| 17 Documentation | each shipped feature | executable examples and accurate model limitations | Partial: existing focused guides retained |
| 18 Distribution | tested supported targets | Debug/Release, MinGW/Linux CI, checksums and smoke tests | Partial: CI/release workflows exist |

## Immediate order

1. Establish a current MinGW core/test baseline without overwriting the existing build.
2. Validate core execution with bounded virtual-time runs, cooperative pause/resume,
   explicit stepping, cancellation and observable scheduler counts (implemented).
3. Validate boundary times, simultaneous events, cancellation and exceptions.
4. Reconcile generated/admitted/dropped/pending packets, bytes, throughput and
   goodput before extending experiment output contracts.
5. Complete missing link jitter/cost and packet identity/class/TTL metadata.
6. Extend existing experiment tooling rather than create a competing runner.

Work proceeds in this dependency order; later partial implementations are not
evidence that earlier acceptance criteria have passed.

On 2026-11-12 the user explicitly requested committing the initial core stage and
moving to milestone 2. Commit `3b22167` contains that tested stage. Experiment
reproducibility now advances without changing metric meanings or claiming that
the outstanding milestone 1 metric/model requirements are complete.
