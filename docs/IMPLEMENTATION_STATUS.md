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
