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
