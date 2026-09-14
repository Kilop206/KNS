# Runner results and execution contract

Issues #133, #134, #135 and #136 are valid on `0f3e20a`.

## Requirements

Engine CSV schema version 1 retains existing counters and appends
`schema_version`, `simulation_duration_s` (logical clock) and `seed`.
The runner requires one well-formed row with finite nonnegative metrics and
supported version. Missing/incompatible output is a failed run, never zero data.
Delivery/loss rates divide their counters by sent packets (zero when none sent).
Throughput divides delivered packets by logical duration; zero duration has no
defined throughput. Host runtime is reported separately as wall-clock duration.

Timeout is measured from launch using a monotonic clock in both scheduling and
draining. Poll all children, so a hung first child cannot hide completed work.
On timeout terminate/reap the process tree (Windows taskkill /T, POSIX session).
Diagnostics distinguish startup, process, timeout and stats failures. Preserve
reports for failed batches and return nonzero if any run failed.

## Plan and acceptance

1. Add versioned producer/consumer schema and logical throughput with unit tests.
2. Enforce launch deadlines, process-tree cleanup and batch exit status; test
   hung children, completed later children and mixed results with simulator stubs.
3. Build and run CTest, Python tests and a real headless CSV integration test.

Status: specified, implementation pending.
