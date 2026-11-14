# Platform implementation status

## Audit — 2026-11-12

The workspace contains separate products; changes for this request target KNS.
The KNS checkout was clean at audit start. Inspected root/core/app/test CMake
configuration, source and test inventories, README, architecture/event design,
experiment tooling, existing implementation roadmap and CI workflow inventory.

Existing functional subsystems include monotonic virtual time, timestamp/ID heap
scheduling and cancellation, per-engine seeded randomness, cloned topology per
run, routing revision invalidation, duplex bounded queues and RED, TCP lifecycle,
link fault events, simulated services, GUI and legacy headless output. Existing
Python tools run seed/parameter matrices and paired comparisons. These do not
constitute completion of the requested platform milestones.

## Findings and limitations

- Engine lacked a bounded run API, core pause/resume, terminal cancellation and
  scheduler execution statistics; GUI state lived separately in namespace `gui`.
- Event IDs use process-global construction order; cross-thread engine mutation
  is unsupported. Replay guarantees concern logical order in equivalent builds.
- Stats use integer packet counters, with no complete generated/admitted/pending
  ledger or byte/goodput accounting. Source-side queue rejection is not included
  in `packets_sent`; ratios must not be advertised as delivery probabilities.
- Queue slots persist through propagation. Jitter, explicit routing cost and
  packet class/TTL/identity remain milestone 1 work.
- CLI headless execution still requires graphical build dependencies.
- No complete versioned scenario/assertion contract, industrial models, trace
  import, capacity planner or isolated netem adapter was established by audit.

## Verification environment

- Existing `build` uses Visual Studio 18 2026 at an unavailable installation;
  `cmake --build build --parallel 2` fails before compilation.
- Existing CTest was attempted, but references missing `C:/Program Files/CMake`
  and old Python paths; cached executable results are not current-source proof.
- Graphify query failed: its configured Python 3.13 interpreter is inaccessible.
  Direct source inspection was used instead of claiming graph-derived evidence.
- A separate `build-core-mingw` configures successfully with GNU 16.2, Ninja,
  C++20, GUI disabled, tests enabled, and cached Catch2/json sources. No dependency
  download was required. Python was not discovered, so Python CTests are excluded.

## Progress and next work

Milestone 1 is partial. Implemented bounded inclusive-horizon execution, a finite
event budget, cooperative pause/resume, step while paused, terminal cancellation,
terminal callback failure with exception propagation, recursive dispatch guards,
pending-event inspection and dispatched/completed/failed counters. Validation
rejects terminally cancelled or failed runs. Existing `run()` and `processEvent()`
remain compatible for callers that do not use controls.

Added eleven C++ cases for boundary ties/newly scheduled ties, zero-time recurrence,
pause/step/resume, cancellation, exceptions, reentrancy, invalid horizons, cancelled
events, terminal validation, physical pending-packet preservation and
continuous-versus-segmented seeded multihop TCP.
The headless CLI exposes time/event limits with explicit incomplete status,
partial CSV preservation, exit code 3 and an integration CTest. Model semantics
and a runnable C++ example are in `simulation-model.md`.

Executed evidence:

- Fresh MinGW core build before implementation succeeded; 357/357 CTest passed
  (`build-core-mingw/baseline-ctest.log`).
- Core build after implementation succeeded; 367/367 CTest passed
  (`build-core-mingw/implementation-ctest.log`). This includes all ten new cases.
- Python unittest discovery initially encountered sandbox temporary-file errors;
  rerun outside the sandbox succeeded: 37 tests, 2 skipped because KNS_TEST_EXE
  was not set. Existing fake-process unit fixtures are not simulation evidence.
- `git diff --check` passed after the core changes.
- Final application + core + test build succeeded with GNU 16.2/Ninja/C++20.
  `ctest --test-dir build-core-mingw --output-on-failure --parallel 2
  --output-log build-core-mingw/full-ctest.log` passed 416/416 tests, including
  the eleven new C++ cases and `headless_execution_limits`. This final run
  includes cooperative cancellation's follow-up scheduling fix.
- Python unittest discovery rerun with KNS_TEST_EXE pointing to the newly built
  `build-core-mingw/app/KNS.exe` passed 37/37, with no skips. This includes actual
  CSV export and profile execution against the simulator from another directory.
- TLS was explicitly disabled for this local application build because MinGW
  OpenSSL headers are unavailable. No HTTPS behavior is claimed from it. GUI
  interaction, Linux execution and sanitizers are not verified in this session.

Changed files: execution contracts in `ExecutionControl.hpp`, engine header and
implementation, `app/main.cpp`, application CMake and `RunHeadlessLimits.cmake`,
`ExecutionControlTests.cpp`, README, architecture/event/experiment/model guides,
this status and platform roadmap. `.gitignore` excludes the isolated local build.
The original build and ecosystem implementation roadmap were preserved.

Next dependency: finish metric conservation and byte/goodput definitions, link
jitter/cost, packet metadata and seed/stream reporting, before certifying milestone
1 or expanding the existing experiment runner. All later milestones retain their
audited partial/pending status; no scaffolding has been counted as completion.

## Milestone 2 increment — 2026-11-12

The user explicitly requested committing the initial stage and moving to the
next milestone. Commit `3b22167` preserves that verified core/CLI work. The
remaining milestone 1 issues above are still open; this increment does not
change aggregate metric meanings or certify the core milestone.

Implemented on the existing Python experiment runner:

- Version 1.0 profiles, with backward-compatible version omission; explicit
  unsupported-version rejection and a published JSON Schema with CI fixtures.
- Full matrix preflight: default 1000 cases, configurable up to 10000. Lists and
  ascending decimal-grid ranges support bandwidth, delay, loss and queue capacity;
  combinations are bounded before materialization. Seeds/payload/routing/TCP
  settings multiply the matrix. Signed-int payload/queue size limits are enforced.
- Portable `manifest.json` embeds normalized variants/topology/timeout, canonical
  configuration SHA-256, binary hash, runner hashes, source revision/version,
  platform/Python details, UUID, timestamps, status and result references.
- `--replay` reconstructs inputs in a new directory from the manifest, independent
  of deleted/moved original inputs. Hash/version validation rejects corrupted
  inputs; a changed binary needs explicit override and remains recorded.
- Existing-directory evidence is preserved, including prior manifests. Manifest
  writes use atomic replacement; failures remain distinguishable from completion.
  Individual benchmark cases also receive UUIDs and UTC start/end timestamps.
- A documented 24-case bandwidth/delay/queue example and replay commands.

Changed files: `scripts/experiment_profile.py`, new `experiment_manifest.py` and
`experiment_sweep.py`, `benchmark_suite.py`, expanded profile tests, experiment
guide/example, profile schema/fixtures, architecture, experiment notes, roadmap
and this status. No C++ engine or CSV metric contract changes in this increment.

Executed evidence:

- `cmake --build build-core-mingw --parallel 2` succeeded (no native rebuild needed).
- CTest: 416/416 passed; `build-core-mingw/milestone2-ctest.log`.
- Python unittest discovery with the current KNS executable: 45/45 passed, no
  skips. The profile suite now has 17 tests, including actual engine replay,
  deleted sources, moved bundles, tamper detection, changed-binary rejection,
  failure manifests and matrix/range limits.
- Contract validation: 9 schemas, 26 fixtures, 4 topologies passed. Existing
  `requirements-contracts.txt` dependencies were installed into the ignored build
  directory; validation needed unsandboxed read access to their installed ACLs.
  No new mandatory runner dependency was introduced.
- Executed `KNS Experiments/capacity-sweep.json`: 24/24 successful cases and 22/22
  paired comparisons. Replayed its saved manifest in `milestone2 replay`:
  24/24 successful cases. Verified all 24 simulator CSVs byte-for-byte, equal
  configuration hashes and distinct case UUIDs. Evidence lives under ignored
  `build-core-mingw/milestone2-capacity` and `build-core-mingw/milestone2 replay`.
- `git diff --check` passed.

Milestone 2 remains partial: a complete scenario contract with configurable
traffic, virtual duration, requested metrics/assertions/output policy is pending.
Traffic-rate/device-count sweeps and complete build/toolchain provenance also
remain pending. Profiles currently run the established workload to completion;
wall timeout is distinct from virtual duration. Replay is from configuration,
not a saved engine checkpoint. Source version/revision describes the checkout;
the recorded binary hash identifies the executable. Existing core metric/model
limitations and local TLS/Linux/GUI verification limits continue to apply.

## Milestone 3 increment — 2026-11-12

Advanced to Fault Lab at the user's explicit request. This increment remains
simulation-only and retains the existing LinkFailureEvent and legacy CLI syntax.

Implemented:

- `engine/experiments/FaultLab` validates versioned plans before scheduling link
  down/up, bandwidth, propagation-delay and loss changes as normal owned events.
  Repeated events support flapping. Targets resolve from original link-array
  index to stable runtime identity, so parallel links are supported safely.
- Explicit `complete_accepted_hops` policy: accepted/queued reservations retain
  timing. New sends use current configuration and synchronous routing refresh.
  Missing runtime targets fail execution instead of claiming applied faults.
- Observations at start, every fault timestamp and user-supplied times capture
  counters, pending packets, queue occupancy, link state/parameters and route
  traces. Reports derive interval deltas/throughput/DATA latency and separate
  sampled link availability from probe connectivity outage intervals. Open
  intervals are censored; application service recovery remains null/unmeasured.
- `--fault-plan <json>` on headless CLI, with a 4 MiB input limit and adjacent
  `<output>.faults.json`. Bounded runs retain pending-work diagnostics.
- Profiles embed per-variant plans; generation, manifests, replay, benchmark
  execution and paired comparisons retain fault context. The benchmark runner
  requires matching applied-event and observation evidence before accepting a
  completed case. Runtime/snapshot limits prevent accidental huge retained logs.
- Fault plan JSON Schema/fixtures, a three-treatment/two-seed example, a detailed
  model guide and eight native plus three Python regression/integration cases.

Changed files: new Core FaultLab header/source and native tests; application CLI;
Python plan preflight/report validation, profile/benchmark/comparison/manifest
integration and profile tests; fault/profile schemas and fixtures; `fault-lab.json`;
README, experiment guide, architecture, roadmap, `fault-injection.md` and this log.
No new dependency, actual network mutation or inert Studio control was added.

Executed evidence:

- MinGW GNU 16.2 / C++20 application, Core and tests compiled successfully.
- `ctest --test-dir build-core-mingw --output-on-failure --parallel 2
  --output-log build-core-mingw/milestone3-ctest.log`: 424/424 passed, including
  all eight Fault Lab cases. Cases cover accepted/queued timing across failure
  and degradation, alternate routes, parallel links, restoration, invalid-plan
  rejection, same-time ordering, censoring, flapping and missing runtime targets.
- Python discovery with the built KNS: 48/48 passed, no skips. Actual CLI tests
  include invalid-plan rejection, plan evidence checks and byte-identical replay.
- Contract validator: 10 schemas, 28 fixtures and four topologies passed using
  the existing isolated build dependencies with required unsandboxed ACL access.
- Final bundled profile: six successful runs, four matched pairs. The first
  trial revealed traffic had already drained before the fault; the shipped
  example was corrected to inject at 0.025 s and restore at 0.08 s. Final control
  dropped zero packets; outage runs each dropped 21, including drops observed
  during the fault; degraded runs dropped 4/3 for seeds 42/43. These are simulation
  counters under this workload, not estimates for a physical deployment.
- Replayed final manifest in another directory: six successful runs. Verified
  all six simulator CSVs and six fault JSON reports byte-for-byte. Artifacts:
  `build-core-mingw/milestone3-active` and `build-core-mingw/milestone3 active replay`.
- `git diff --check` passed. Local TLS remains explicitly disabled; Linux,
  sanitizers and interactive Studio verification were not executed in this session.

Milestone 3 remains partial: node/device failure and recovery, overload and
dedicated gateway models, conditional/random triggers, per-flow impacts, measured
service recovery and Studio scheduling UI are pending. Route recomputation is
synchronous and does not measure distributed convergence. Observation windows
are sampled callback intervals and retain the earlier packet metric limitations;
they do not establish unique-payload goodput or unconditional delivery probability.

## Milestone 4 increment — 2026-11-12

Advanced to Compare at the user's explicit request, extending the existing
recorded-artifact comparator without changing native simulation or metric meaning.

Implemented:

- Per-pair reference/candidate values, absolute and percentage deltas and observed
  direction. Percentage deltas remain null at zero baseline; their summary averages
  defined paired percentages with an explicit contributing count.
- Per-workload-group seed mean/median/min/max, sample standard deviation, side
  means and conditional nominal 95% paired Student-t intervals. Normality/seed
  independence are assumptions, not established facts. Critical values use the
  NIST table rounded to three decimals, with degrees of freedom capped at 30.
  Single samples and constant observed deltas have no asserted interval.
- Offline self-contained HTML with inline CSS/SVG delta distributions, means and
  conditional intervals, text/color directions and explicit limitations. Existing
  paired CSV/Markdown remain; JSON comparison version is 1.1 and a long-form
  summary CSV records uncertainty/relative sample counts.
- `--against` compares selected variants from independent bundles in a required
  new output directory, retaining source configuration/provenance and unavailable
  pairs. Explicit reference/candidate routing selections are supported; all other
  workload dimensions still pair exactly. Source bundles remain untouched.
- Bounded case-matrix preflight, escaped artifact content, restrictive HTML CSP,
  no scripts/external chart assets, and documented differences between observed
  direction, conditional uncertainty, statistical significance and causal claims.
- A reproducible five-seed comparison of a three-node chain with an added direct
  redundant link, under identical endpoint roles/TCP settings/fault schedule and
  observation times. This combined treatment changes redundancy and path delay;
  it does not isolate a redundancy-only causal effect.

Changed files: comparator and its new statistics/artifact/HTML modules; expanded
comparison tests; runner source-hash manifest list; four topology/profile example
files; README/experiment guide, architecture, roadmap, `compare.md` and this log.

Executed evidence:

- `cmake --build build-core-mingw --parallel 2` succeeded; no native rebuild was
  required. CTest: 424/424 passed (`build-core-mingw/milestone4-ctest.log`).
- Python discovery with the actual KNS executable: 55/55 passed, no skips. The
  comparator suite now contains 17 tests, including known paired interval solution,
  zero-reference and per-pair percentage semantics, missing seeds, failed cases,
  conditional-interval absence, preflight bounds, explicit routing pairs, preserved
  source/destination evidence and escaped offline HTML/SVG.
- Contract validator: 10 schemas, 28 fixtures, four bundled app topologies passed;
  required unsandboxed access to existing contract-dependency ACLs was reused.
- Six real fault-profile executions succeeded, producing HTML and four matched
  variant pairs in `build-core-mingw/milestone4-compare`.
- Chain and redundant topology profiles each executed five real cases successfully.
  Cross-bundle comparison matched 5/5 workload pairs. Final derived report is
  `build-core-mingw/milestone4-topologies-final/comparison.html`, alongside JSON,
  paired/summary CSV and Markdown. Structural checks verified seven embedded SVGs,
  CSP, no external assets/scripts and unavailable intervals for constant deltas.
  Case diagnostics remain in `milestone4-chain` and `milestone4-redundant`.
- Visual browser QA was attempted twice (including runtime reset) but the CUA
  tool failed before opening the file: `failed to write kernel assets: ...
  (os error 3)`. No screenshot/layout approval is claimed. HTML content/escaping
  and structural behavior were checked; actual rendered layouts remain unverified.
- `git diff --check` passed. TLS/Linux/sanitizer/interactive Studio limits from
  earlier stages remain unchanged.

Milestone 4 remains partial: aggregate inputs do not supply unique goodput, packet
latency median/p95/p99, jitter, utilization or service-recovery metrics. General
traffic-model comparisons, additional routing algorithms, fault time-series/recovery
comparison and Studio Compare View remain pending. Different bundle parameters do
not establish identical offered load; reports retain that limitation and do not
rank a global winner or infer causality automatically. Virtual duration includes
TCP cleanup timers/observations and can differ despite equal observation schedules.

## Milestone 5 increment — 2026-11-12

Advanced to runtime Analytics at the user's explicit request. Existing topology
analysis and GUI observers were preserved; collection is optional and off by default.

Implemented:

- Native `MetricsCollector` injected before traffic, with transmission/delivery
  hooks and ordinary virtual-clock sampling events. Collection has a declared
  horizon, independent of whole-run completion. No RNG draws, background worker,
  LLM, actual network operation or GUI dependency was added.
- Bounded time series for packet throughput, first-hop admissions, path-drop
  rates, DATA latency, link configuration, directional/shared queues and link
  serialization utilization. Physical busy intervals are clipped to sample
  windows, excluding propagation and respecting full/half/simplex slots. Coverage
  loss, missing links and incompatible mode windows are explicit; unavailable
  utilization is null rather than an invented zero.
- `LatencyStatistics` accumulates Welford moments with long-double intermediates,
  count/min/max and all-observation fixed-bin histogram counts. Median/p95/p99 use
  bounded recent deliveries and declare scope/retained/discarded counts. No
  observations/single observations have appropriate null statistics.
- Latency/delivery/wire-byte aggregates globally and in capped flow/destination
  groups. Overflow deliveries stay in global counters and are explicitly counted
  as ungrouped. Time series, configuration-change logs and pending reservations
  have bounded retention. Link summaries retain peak evidence from valid windows.
- Three deterministic heuristic rules: high directional serialization utilization
  (>=0.9), high directional/shared queue fraction (>=0.8) and observed path drops
  (>0). Recommendations include evidence, relevant thresholds/timestamps, suggested
  experiments and model limitations; they are not causal bottleneck verdicts.
- Headless `--analytics <json>` exports `.csv.analytics.json`. Profiles embed
  collection config, manifests hash/persist it, replay reconstructs it, and the
  benchmark runner requires matching complete collection evidence. Added schema,
  positive/negative fixtures and a controlled congestion example.

Changed files: new native latency-statistics/collector headers and implementations;
engine/destination-delivery hooks; application CLI; Python analytics preflight and
report validation; profile/benchmark/manifest integration and tests; config/profile
schemas/fixtures; `analytics-lab.json`; README/guides, architecture, roadmap and this
status. The simulator aggregate CSV contract and Compare metric meanings remain
unchanged; retained-scope statistics are not silently merged with full-run metrics.

Executed evidence:

- GNU 16.2 MinGW/C++20 application/Core/tests compiled successfully.
- CTest: 433/433 passed, including nine Analytics cases
  (`build-core-mingw/milestone5-ctest.log`). Cases verify known moments/percentiles,
  large-offset variance stability, serialization/propagation separation, duplex
  normalization, retention/group/reservation coverage, sampled faults, modeled
  random-loss serialization, config validation and seeded TCP equivalence.
- Python discovery with current KNS: 58/58 passed without skips. Added three tests
  covering invalid configs, actual CLI collection/retention/evidence rejection,
  byte-identical replay and equal native CSVs with/without collection when the
  observation horizon lies within the original run.
- Contract validator: 11 schemas, 30 fixtures and four app topologies passed.
  Existing unsandboxed temporary-process/contract-package ACL accommodations were
  reused; no new runner dependency was installed or made mandatory.
- Final `analytics-lab.json`: four successful actual simulation cases, two matched
  pairs, 201 samples per case through virtual second 1. Control peak directional
  utilization was about 0.4928 and queue fraction 0.625, with no diagnostic rule
  triggered. The congested link reached utilization/queue fraction 1 and triggered
  all three relevant rules. Only two DATA packets arrived in that window in each
  congested case versus 40 in each control: percentile values are descriptive
  observations, not SLA or capacity guarantees.
- Replayed final manifest: four successful cases. Verified all four Analytics
  JSON reports and four simulator CSVs byte-for-byte, 201 timestamps per report,
  histogram count conservation, utilization bounds and current executable SHA-256.
  Evidence: `build-core-mingw/milestone5-final` and
  `build-core-mingw/milestone5 final replay` (ignored build artifacts).
- `git diff --check` passed. No new interactive Studio/visual QA was performed.
  Native TLS remains locally disabled; Linux/sanitizers remain unverified here.

Milestone 5 remains partial: device processing rates, unique goodput, per-flow
path-drop attribution/deadlines, jitter, automatic imbalance/congestion-pattern
diagnosis, streaming storage, richer rule sets and Studio Analytics Dashboard are
pending. Current group keys represent modeled session/direction endpoints and
destination deliveries. Queue entries still include propagation; intervals and
configuration-change markers are sampled callback observations. Retention scopes
and virtual horizons must be respected when interpreting or comparing outputs.

## Milestone 5 continuation — 2026-11-13

Implemented and integrated:

- Absolute latency change between consecutive DATA deliveries within each tracked
  flow/direction, with global/window/flow statistics and bounded retention. The
  first delivery contributes no pair, interleaved flows never form pairs and
  destination groups do not mix flows. This is a declared variation measure,
  not a protocol-specific jitter estimator or unique application goodput.
- Repeated joint serialization/queue pressure: three consecutive valid windows
  above documented thresholds trigger a deterministic recommendation. Scalar
  counts, durations and longest streak survive time-series trimming. Missing
  utilization evidence breaks the streak. Evidence depends on sampling interval
  and does not establish sustained congestion or causality.
- Semantic validation before benchmark acceptance and offline export: finite
  values, retention/histogram conservation, group packet/DATA/byte totals,
  consecutive-flow pair counts, interval continuity, rates, utilization bounds
  and explanation fields. Consistency checks do not authenticate external data.
- Dependency-free offline HTML, series/link/group/histogram CSV and recommendation
  Markdown export to a new directory, preserving the source. Charts use virtual
  time, explicit missing values, fixed 0–1 utilization axes and escaped labels.
  Older schema-1.0 reports without variation remain readable. Partial reports,
  invalid evidence and inputs over 64 MiB are rejected. HTML charts at most 16
  original links; CSV preserves all retained links.

Changed files: native collector and analytical regression tests;
`analytics_config.py`, new `analytics_validation.py`, `analytics_report.py` and
report tests; manifest runner hashes and CMake test registration; READMEs,
Analytics guides, architecture, roadmap and this status. See
[analytics-analysis.md](analytics-analysis.md) for definitions and usage.

Executed evidence:

- MinGW/C++20 application/Core/tests build succeeded. CTest: **435/435 passed**
  (`build-core-mingw/milestone5-analysis-ctest.log`). Eleven Analytics cases
  include 11/21/6 ms packet latencies yielding 10/15 ms same-flow changes,
  interleaved-flow exclusion and repeated/transient/unknown pressure checks.
- Python discovery with the actual KNS executable: **63/63 passed**, without
  skips (`build-core-mingw/milestone5-analysis-python.log`). Five new report cases
  cover actual-engine export, deterministic derived files, legacy compatibility,
  malformed/partial evidence, source preservation, escaping and chart gaps.
- Contract validation passed: **11 schemas, 30 fixtures, four bundled topologies**.
  Existing temporary-process/dependency ACL accommodations were reused; no new
  mandatory dependency was installed.
- Final Analytics example and manifest replay each completed four successful
  cases. All four simulation CSVs and four Analytics JSON reports were identical
  byte-for-byte; each report has 201 samples. Both manifests match the current
  executable SHA-256. Both control runs observed 40 DATA deliveries/38 variation
  pairs and no pressure windows; congested runs observed two DATA deliveries/one
  pair and 25 joint-pressure windows (125 ms). These small counts are descriptive,
  not statistical capacity or SLA guarantees.
- All six derived files were byte-identical between original/replayed reports.
  HTML parsed successfully and contains embedded SVG with no scripts. Evidence:
  `build-core-mingw/milestone5-analysis-final` and
  `build-core-mingw/milestone5 analysis replay`; HTML is under each `report/`.
- `git diff --check` passed. Browser visual QA, interactive Studio, native TLS,
  Linux and sanitizers were not verified in this continuation. HTML parsing and
  escaping tests do not establish visual layout correctness.

Milestone 5 remains **partial**. Pending work includes device processing rates,
unique goodput, protocol-specific jitter, per-flow path-drop/deadline attribution,
automatic utilization imbalance analysis, streaming storage and Studio Analytics.
Existing queue propagation semantics, sampled boundaries and explicit retention
limits still apply. No feature is claimed complete solely from report scaffolding.

## Experiment provenance portability — 2026-11-13

The runner no longer requires a root `CMakeLists.txt` merely to record optional
source-version metadata. Packaged/reorganized layouts record `source_version:
unknown` when the file is absent, unreadable or has no recognized project version.
Executable SHA-256 identity and replay mismatch rejection remain mandatory when
an executable is supplied. No build sources or dependencies are restored by this
change. Files: `experiment_manifest.py`, four focused unit tests and CTest
registration. All four tests passed with the current Python runtime. Native build
reorganization by the user is preserved; independent native verification is being
prepared in the ignored `build-core-mingw` tree.

## TCP payload accounting — 2026-11-13

Added receiver-release counters and virtual-time TCP payload goodput globally,
per sample, per flow and per destination, with explicit overflow byte accounting.
The hook runs after accepted DATA receive processing; unsigned sequence-space
advancement excludes duplicates and counts buffered bytes only when gaps close.
It adds no transport scheduling or random draws. See [tcp-goodput.md](tcp-goodput.md)
for the distinction between transport release and application reads.

Files: collector/engine interfaces and implementations, DATA receive hook,
Analytics regression tests and model documentation. The isolated MinGW/C++20
build succeeded; all 12 Analytics cases passed (144 assertions), including
duplicate/out-of-order/empty/unknown-session packets, 32-bit wrap and group
overflow. Evidence: `build-core-mingw/goodput-native.log`. The old build cache
referenced dependencies intentionally removed by the user; verification used
`build-core-mingw/verification-src` and `verification-build` without restoring
the root build file. Application-read goodput, device processing and Studio
integration remain pending; transport-release accounting is now implemented.

## Goodput validation and export — 2026-11-13

The offline Analytics exporter now includes TCP release counters and goodput
in HTML cards/charts and group/time-series CSV. Older reports preserve missing
values. Semantic checks reject inconsistent rate denominators, non-monotonic
cumulative bytes and group/overflow conservation errors. Files:
`analytics_validation.py`, `analytics_report.py` and report regression tests.
All six exporter tests passed with the actual isolated-build executable, including
seven corrupt-goodput variants and the full congestion example export. Evidence:
`build-core-mingw/goodput-report-tests.log`. The additive group-key validator also
recognizes an explicit `traffic:<flow_id>:<source>:<destination>` namespace for
abstract workloads; it never merges these with TCP session keys.

## Traffic Lab native generators — 2026-11-13

Implemented `traffic::TrafficRun` with periodic, CBR, burst, Poisson, uniform and
custom schedules. Flows share actual routing/serialization/propagation/queue/loss
models, use independent per-flow seeded streams, configurable hop TTL and optional
delivery deadlines. Priority is explicit class metadata; no priority queue is
claimed. A single plan owns the engine's abstract-flow namespace. Plans validate
fully before scheduling; one generation event per active flow and a global packet
budget bound work. Generated/delivered/dropped/pending scalar ledgers follow packet
copies and preserve pending states on interruption. Application/TCP protocol
semantics are not synthesized. See [traffic-models.md](traffic-models.md).

Files: new TrafficRun API/implementation and PacketWorkload lifecycle metadata;
engine plan guard, packet arrival/drop/TTL hooks, Analytics traffic namespace and
seven native regression cases. MinGW/C++20 build passed; all seven Traffic cases
passed (306 assertions), including actual queue competition, analytical timing,
deadline accounting, source/intermediate/TTL rejection, budgets/cancellation,
stream isolation, sequence determinism and duplicate-plan rejection. Evidence:
`build-core-mingw/traffic-native-tests.log`. Full suite before the final plan-guard
checks passed 443/443 (`traffic-core-ctest.log`); final combined checks follow the
CLI/contract integration. Milestone 6 is partial; application dependency models,
arbitrary distributions, sweeps/import and Studio remain pending.

## Traffic Lab CLI and contracts — 2026-11-13

Headless `--traffic-plan` accepts versioned JSON and writes a per-flow terminal
ledger beside the legacy CSV. Explicit plans replace automatic TCP generation;
Analytics/Fault Lab remain composable. Duplicate arguments and invalid plans fail
before execution. Time/event/generation truncation returns 3 with an incomplete
diagnostic; a drained workload returns 0 even when all packets were rejected.
This does not certify any delivery threshold. Reports record separate plan/network
seeds and never count abstract payload as TCP goodput.

Added strict schema/fixtures, Python preflight/report conservation checks, CTest
registration and a six-profile shared-backbone example under
`KNS Experiments/traffic-lab`. Three Python tests passed without skips, including
real CLI runs, byte-identical traffic replay, all schedule kinds, malformed input,
paths with spaces, pending/budget reports and all-loss execution. Evidence:
`build-core-mingw/traffic-python-tests.log`. Contract validator passed 12 schemas,
32 fixtures and four bundled app topologies. Native application/test build passed
in the isolated build; TLS, Linux and Studio remain unverified. Usage and model
limits are in [traffic-models.md](traffic-models.md).

## Traffic experiment integration — 2026-11-13

Profiles embed traffic plans in configuration hashes, generated bundles and
portable replay. The benchmark passes the plan and requires a complete, conserved
traffic ledger. Inactive TCP size/control sweeps are rejected. Compare declares
the workload model, retains legacy CSV denominators, records cross-bundle plan
hashes and reports input mismatch instead of asserting equivalent offered load.
The runner hash includes traffic validation code. Added a controlled bandwidth
upgrade profile, integration tests and model/usage documentation.

Final verification for this increment:

- MinGW/C++20 Core/application/tests build passed; CTest **443/443 passed**
  (`build-core-mingw/traffic-final-ctest.log`).
- Full Python suite with current executable: **74/74 passed**, without skips
  (`build-core-mingw/traffic-integrated-python.log`). New tests verify generated
  plan preservation, inactive-axis rejection, actual four-case manifest replay
  and mismatched workload provenance in cross-bundle comparisons.
- Contracts: **12 schemas, 32 fixtures, four bundled topologies passed**.
- Saved six-flow experiment and replay each completed four cases with two paired
  comparisons. Verified four CSVs, four traffic ledgers and four Analytics reports
  byte-for-byte; both manifest binary hashes match the executed binary. Each case
  generated 326 packets. Control delivered 242/dropped 84 with 90 late telemetry
  packets; upgrade delivered 316/dropped 10 with zero late telemetry packets.
  The two network-seed repeats coincide because the topology has no random loss;
  these are controlled simulation observations, not independent workload samples
  or real-network capacity guarantees.
- Evidence: `build-core-mingw/traffic-final`, `traffic final replay` and their
  manifests/reports. Offline Analytics HTML was exported under `traffic-final/report`.
  Browser visual QA, TLS, Linux/sanitizers and Studio remain unverified.

Milestone 6 remains partial: no causal application dependency/request-response
model, arbitrary distributions, traffic-parameter sweeps, trace replay or Studio
controls are claimed. Milestone 5 still lacks device processing rates, application
reads, protocol-specific jitter and additional analyses. User build edits were
preserved; the temporary verification configuration stays in ignored artifacts.

## Causal application workload increment — 2026-11-13

Added optional application abstractions for request/response, HTTP/API/database/
microservice messages, messaging, telemetry and streaming. Configured responses
are emitted only after actual request delivery and an explicit processing timer,
then traverse the reverse network path. Responses share the packet budget and
never advance source-schedule ordinals. There is no protocol parsing, implicit
retry, finite device processor, application dependency graph or MQTT guarantee.

Reports distinguish source/response packet stages and operations started,
completed, failed, pending, on time and late. Round-trip statistics reuse bounded
LatencyStatistics (256 recent completions for quantiles, all-observation moments/
histograms). Request/response wire sizes may differ; validation checks stage/byte/
operation conservation and distinguishes packet from operation deadlines. A
response omitted by the global budget leaves an incomplete operation, not a
successful exchange. Details: [traffic-models.md](traffic-models.md).

Changed files: native workload metadata/runtime and regression cases; traffic
preflight/report validation and Python integration tests; schema/positive/negative
application fixtures; mixed application example; model guide, roadmap and status.

Executed evidence:

- MinGW/C++20 build passed; all ten native Traffic cases passed (375 assertions).
  Known request/reply paths plus a 3 ms processing timer produce an 8 ms round
  trip in the two-node model and 10.03 ms in the CLI example topology. Tests cover
  request/reply loss, budget-truncated operations and unchanged source schedules.
- CTest **446/446 passed** (`build-core-mingw/application-ctest.log`).
- Full real-engine Python suite **76/76 passed**, no skips
  (`build-core-mingw/application-python-tests.log`).
- Contracts: **12 schemas, 34 fixtures and four bundled topologies passed**.
- Mixed seven-application experiment and manifest replay each completed four
  actual cases/two comparison pairs. Verified 12 raw CSV/traffic/Analytics files
  byte-for-byte. Each case generated/delivered 260 packets, completed 36 exchanges
  and dropped none. Control had nine late operations with configured deadlines;
  the bandwidth upgrade had zero. These are scoped model observations with fixed
  processing delays, not real application performance or statistically independent
  workload samples. Evidence: `build-core-mingw/application-final` and
  `application final replay`.

Milestone 6 remains partial: arbitrary dependency graphs/distributions, traffic
parameter sweeps, trace replay and Studio remain pending. Device processing
capacity and service/application-read goodput are still not modeled. Existing
Windows/TLS/Linux/visual-QA limitations remain as recorded above.

## Bounded workload parameter sweeps — 2026-11-13

Added joint Cartesian link/traffic axes using existing bounded list/range
expansion. Whitelisted flow parameters include packet size, offered rate/timing,
class/TTL/deadline and existing application reply size/processing/deadline.
Normalized named overrides and every effective plan are validated before output.
Variants retain independent plan files; manifests preserve the immutable base plus
overrides and replay reconstructs them. Unknown flows/paths, conflicting axes,
model switches, invalid effective configurations and excessive combinations fail
preflight. Finite-number checks also reject oversized integer sweep inputs cleanly.
Compare fingerprints effective workload configurations within/across bundles,
including overrides; differing loads are explicit and do not imply causal pairing.

Files: traffic validation/override helper, sweep/profile/benchmark integration,
Compare provenance, schema/fixtures/tests, load-sweep example and documentation.
Full real-engine Python suite **79/79 passed**, no skips
(`build-core-mingw/traffic-sweep-final-python.log`). Contracts passed **12 schemas,
36 fixtures and four topologies**. Native sources were unchanged since the
446/446 CTest run. Actual four-case sweep and replay produced 12 identical raw
CSV/traffic/Analytics files. Two rates generated 251 and 326 packets; at 1 Mbps
they delivered 228 and 242, versus 246 and 316 at 10 Mbps. These are model-specific
observations; Compare retains both distinct workload hashes. Evidence:
`build-core-mingw/load-sweep-final` and `load sweep final replay`.

Traffic parameter sweeps are now implemented. General operation-dependency graphs,
additional distributions, trace import and Studio controls remain pending; earlier
scientific/model/platform limitations still apply.

## Full-horizon utilization contrast — 2026-11-13

Analytics now retains the time integral of each window's busiest-slot fraction
and valid duration independently of sample retention. Link summaries expose a
bounded time-weighted mean (null without coverage). A deterministic heuristic
compares at least two links with complete same-horizon evidence, flagging mean
fractions >=0.8 alongside <=0.2. Its recommendation includes link identities,
thresholds, denominator and limitations; different capacity/demand/route roles
remain alternative explanations. It does not diagnose causality or change routes.

Files: native collector, analytical test, summary-domain validator and regression
test; Analytics guides, roadmap/status. Tests verify an actually busy/idle pair
despite retaining one sample and exclude short peaks, partial runs and discarded
reservation coverage. Build passed; CTest **447/447 passed**
(`build-core-mingw/imbalance-ctest.log`); full real-engine Python **80/80 passed**,
no skips (`imbalance-python.log`). Schema contracts were unchanged since the
12-schema/36-fixture/four-topology validation. Device processing, routing
concentration, protocol-specific jitter and Studio remain pending in Analytics.

## Workload assertions and CI diagnostics — 2026-11-13

Added `scripts/validate_result.py` for explicit versioned assertions over complete
Traffic Lab reports. It validates input conservation/completion first, supports
global/per-flow packet and operation counts/ratios, deadline delivery and per-flow
round-trip mean/retained percentiles. Units, denominators, sample scopes and
unavailable values are explicit. Empty populations never pass comparisons.
Outputs are deterministic JSON, JUnit XML and Markdown in a new directory, with
source/rule/configuration/validator SHA-256 identities. Exit codes: 0 pass, 1
threshold/unavailable metric, 2 input/output error. No simulator rerun, external
service or real-network operation occurs in the validator.

Changed files: validator and tests; schema/positive/negative fixtures; example
rules; CTest registration; Linux deterministic CI reference/artifact steps; guides,
README, roadmap and this status. The CI reference uses generated-packet ratios,
not wall-clock performance thresholds. Remote GitHub Actions was not executed.

Executed evidence:

- Full Python with the actual simulator: **83/83 passed**, no skips
  (`build-core-mingw/result-validation-python.log`). Regression tests execute the
  same offered workload at 10 and 1 Mbps: the first passes delivery >=0.9, the
  second exits 1 and emits a failed JUnit assertion. Tests also preserve sources/
  existing outputs and reproduce all three verdict files byte-for-byte.
- Configured the isolated CMake build with the known working Python executable,
  using cached dependencies only. Build passed. **CTest 454/454 passed**, comprising
  447 native/headless tests and seven registered Python categories
  (`build-core-mingw/validation-final-ctest.log`). The final validator identity/
  determinism extension also passed its targeted CTest category afterward
  (`validation-determinism-ctest.log`).
- Contracts: **13 schemas, 38 fixtures and four bundled topologies passed**.
- Saved local equivalent of the CI reference under
  `build-core-mingw/validation-reference`: four successful cases/two pairs.
  Explicit validator invocation on the upgrade returned 0 with no failures;
  invocation on the controlled lower-bandwidth case returned 1 with one failed
  assertion. JSON/JUnit/Markdown evidence is under `validation-pass` and
  `validation-regression`. This is an expected negative control, not a failing
  implementation test.

Milestone 9 remains partial: automatic baseline-relative rules, scenario-embedded
assertions, richer languages and Studio integration are pending. Core model,
platform and scientific limitations recorded above still apply. No milestone is
declared complete merely because CI configuration or schemas exist.

## Initial Industrial scenarios and measured scale — 2026-11-13

Added a bounded scenario generator with explicit IoT sensor nodes, a forwarding
gateway, telemetry destination and abstract PLC/actuator request-response loop.
It emits versioned normal topology/Traffic Lab/profile inputs and executes three
real variants: reference uplink, timed uplink outage/recovery, congestion. An
optional higher-cost alternate route provides uplink redundancy. Every execution
retains normal manifests/raw metrics and measured wall-time summaries; generated
configuration and measurements remain distinct. The common gateway is still a
single point of failure. MQTT, physical process behavior and node failures are
not synthesized.

Files: `industrial_scenario.py`, three tests/CTest registration, `iiot.md`, README,
roadmap and status. The generator accepts 1..250 sensors within current flow
limits; requests for 1000/10000 fail clearly before output. Those targets remain
pending. Device classes map to existing IoT/router/server capabilities; priority
metadata does not implement priority scheduling.

Executed evidence:

- Actual 100-sensor tests inspect route probes during/after outage, primary/alternate
  hop counts, 1010 generated packets per case, five completed control operations,
  congestion loss and measured process times. Single-uplink outage discarded 200
  sensor messages; alternate routing delivered all 1010 packets and 1000 timely
  sensor messages. Congestion delivered 534 packets with 17 timely sensor messages.
- Measured 10, 100 and 250 explicit sensors in this MinGW/C++20 Debug environment.
  Reference process wall times were 0.171/0.894/2.707 seconds; three-case runner
  totals were 0.814/3.249/9.190 seconds. These single measurements include exports
  and process startup, without performance guarantees or hardware-independent
  thresholds. Memory/event rate were not measured. At 250 sensors the offered
  2.5 Mbps sensor load exceeds the 2 Mbps reference link; 2012/2510 deliveries are
  a capacity outcome. See `iiot.md` for configuration and interpretation.
- Full actual-engine Python **86/86 passed**, no skips
  (`build-core-mingw/industrial-final-python.log`). Build passed; **CTest 455/455
  passed**, including eight Python categories (`industrial-final-ctest.log`).
  Contract schemas/fixtures were unchanged since the 13/38/four-topology check.
- Saved raw evidence: `build-core-mingw/industrial-{10,100,250}` and
  `industrial-100-redundant`. Replayed the 100-sensor manifest and compared all
  ten CSV/Traffic/Analytics/Fault Lab files byte-for-byte with
  `industrial-100-replay`. Offline outage Analytics HTML is under
  `industrial-100/report`. Wall-time measurement summaries are intentionally not
  compared as deterministic artifacts.

Milestone 7 remains partial: MQTT QoS, gateway/broker node failures, richer devices,
1000/10000-sensor targets and Industrial Studio remain pending. Routing-table
storage and per-flow bounds need attention before increasing those targets. No
network privileges or real-network changes were used; remote CI, TLS, Linux and
visual QA remain unverified in this workspace.

## Initial Capacity Planner — 2026-11-13

Added actual bounded sensor-growth/uplink evaluation on the Industrial template.
Decimal upward rounding preserves intended growth counts; every population/link
alternative runs through the existing experiment/manifest pipeline. Explicit
delivery/timely-sensor requirements yield observed verdicts. Optional identical
outages/alternate paths support resilience checks. Summaries report the highest
observed feasible population only, without extrapolation or monotonicity claims.
Configuration, JSON/CSV/Markdown rows and every raw manifest/result are retained.

Optional cost input must cover all evaluated alternatives. No prices/currency are
assumed. A population-specific Pareto frontier uses supplied cost, all-DATA mean
network latency, delivery and timely sensor ratios; equal alternatives remain
coequal. Optional sensor unit cost is a declared component, not total ownership
cost. Without cost data all costs/frontier flags remain unavailable. The schema
and tests use arbitrary explicit test units, not infrastructure market estimates.

Files: planner, tests/CTest registration, cost schema/fixtures, capacity guide,
README, roadmap/status. See [capacity-planning.md](capacity-planning.md).

Executed evidence:

- Full real-engine Python **89/89 passed**, no skips
  (`build-core-mingw/capacity-final-python.log`). Tests verify exact growth rounding,
  preflight matrix/cost limits, Pareto arithmetic, actual differing loads/links,
  raw-evidence preservation and an unpriced redundant/outage alternative.
- Build passed; **CTest 456/456 passed** (`capacity-final-ctest.log`), including
  447 native/headless tests and nine Python categories. The final resilience branch
  was exercised by the registered capacity category in this combined run.
- Contract validation passed **14 schemas, 40 fixtures and four topologies**.
- Saved four-case run: `build-core-mingw/capacity-final`. At 100 sensors both 1 and
  4 Mbps delivered 1010/1010 and met the default 0.99 requirements. At 200 sensors,
  1 Mbps delivered 1026/2010 with timely sensor ratio 0.359; 4 Mbps delivered
  2010/2010 with timely ratio 1. No costs were supplied and no cost frontier was
  invented. Highest observed feasible populations were 100 and 200, not proven
  capacity ceilings. Per-population manifests preserve full inputs/binary identity.

Milestone 8 remains partial: broader application/segment growth, richer cost
models, search/replication, larger populations and Studio planning remain pending.
Industrial/Traffic/Analytics model limitations, unverified remote CI/TLS/Linux and
visual QA remain applicable. No real network changes or privileged actions were
introduced.

## Ordered trace replay foundation — 2026-11-14

Published the previously validated local history to origin/main as requested by
the user, then resumed with small verified commits and a push after each commit.
The clean pre-change baseline passed 456/456 CTest checks
(`build-core-mingw/trace-baseline-ctest.log`).

Added root `replay_events` and replay-only flow schedules to Traffic Lab. One
source event advances the ordered timeline, retaining array order at tied times;
normal network arrivals keep timestamp/event-ID semantics. The full plan is
validated before scheduling, with 10000-event/256-flow limits, known references,
ordered flow-window timestamps and no synthesized application responses. Time/
event interruption preserves the suffix; insufficient packet budget remains
explicitly incomplete even after emitted packets drain. Existing generation,
deadline/byte/drop ledgers and CLI/profile/manifest paths are reused.

Files: native TrafficRun API/implementation and four replay cases; Python
preflight/actual CLI tests; schema/fixtures; trace guide, roadmap and status.
The example fixture is directly executable with --traffic-plan. CSV/JSON
conversion and PCAP/PCAPNG are not claimed by this foundation.

Evidence: MinGW/C++20 build passed; focused replay tests **43 assertions/four
cases passed**, including queue rejection identity for tied sources. Full CTest
**460/460 passed** (`replay-foundation-ctest.log`). Actual-engine Python
**91/91 passed**, no skips (`replay-core-python.log`). Contracts passed **14
schemas, 42 fixtures and four topologies**. No real network traffic or privileges
were introduced. Remote CI, TLS/Linux and browser visual QA remain unverified.

## CSV/JSON metadata trace import — 2026-11-14

Added bounded CSV/JSON conversion into native ordered replay, explicit model-node
mapping and optional execution through the experiment runner. Decimal timestamp
normalization removes the absolute origin; clock collisions are counted. Variable
wire sizes produce native groups with opaque logical-flow correlation. Metadata
records source/importer/plan hashes, sizes, wire totals and source interarrival
statistics/histograms. Labels are suppressed by default; retention is explicit.
Raw traces are never copied/uploaded and topology metadata is not claimed
anonymous. No payload/protocol state or physical topology is inferred.

Limits: 4 MiB per input, 10000 packets, 256 wire groups and bounded sizes/times/
model indices. Duplicate/malformed/non-finite/unsorted/unknown records and excess
limits fail before output; sources and prior results are preserved. Tests cover
CSV/JSON equivalence, nanosecond epoch differences, clock collisions, bad inputs,
label suppression/correlation, group limits and actual portable replay.

Files: importer/four tests/CTest registration; source-trace schema/fixtures;
authored CSV/JSON/topology example; trace guide, README, roadmap and status.
Build passed; **CTest 461/461 passed** (`trace-import-final-ctest.log`); actual
Python **95/95 passed**, no skips (`trace-import-final-python.log`). Contracts:
**15 schemas, 44 fixtures, four topologies passed**. Saved `trace-csv-final` and
`trace-json-final` produced identical plans, simulation CSVs and traffic ledgers:
five generated, four delivered, one actual queue drop; 625/875 offered wire bytes
delivered. Two source flows map to four native wire groups. The analytical
examples are authored inputs, not real capture measurements. Manifest replay also
succeeds after the original private trace is deleted. PCAP/PCAPNG/protocol parsing,
remote CI, TLS/Linux and visual QA remain pending/unverified. Each verified commit
is pushed to origin/main under the user's current instruction.
