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
