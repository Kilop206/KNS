# Verification and real-time guards

Build with C++20 using the existing CMake targets, then run:

```powershell
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --parallel 1
```

Use the path of your configured build. The isolated verification build in this
workspace is `build-core-mingw/verification-build`; the user's original build is
preserved. Optional Python integrations use an explicit interpreter selected at
configuration through `Python3_EXECUTABLE` and pass the actual KNS target path.

`KNS_PYTHON_TEST_TIMEOUT` is a CMake cache setting for the real-time guard around
each Python integration suite: default 600 seconds, accepted integers 1..3600.
Set it when configuring (e.g. `-DKNS_PYTHON_TEST_TIMEOUT=600`). It detects hung
suites; it is not a performance acceptance threshold. The headless CLI smoke test
uses finite 60-second guards per launched command and a 1200-second outer guard.
Each command still checks its expected exit status, virtual execution state and
output files. These values do not alter simulation clock or model parameters.

Process-polling tests control the decision clock while using real subprocesses
and process-tree termination. Batch-success checks allow process startup time;
timeout tests assert statuses and retained reports instead of requiring a fast
host. Native scenario outcomes, routing and ledger invariants remain mandatory.
Industrial scenarios expose their own per-case `--timeout`, recorded in the
manifest; the default production value remains 60 seconds.

Hardware benchmarks must record their environment and are separate from these
functional checks. A timeout is not a valid simulation result. Retain logs and
manifests when diagnosing failures; do not report an expired case as a measured
network limitation. The latest outcomes and known failures are in
[IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md).
