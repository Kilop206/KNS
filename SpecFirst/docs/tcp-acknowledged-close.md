# Acknowledged TCP Workload Completion - Issue #122

## Investigation

Still valid on `tcp` at `3b61999`: generation completion alone triggers automatic
FIN on any ACK, even when application data remains in the send buffer.

## Required Contract

`isComplete()` retains its generation-completion meaning. A separate
`isDataAcknowledged()` predicate requires generation to have started and finished,
and both endpoint send buffers to be empty. Automatic FIN requires that predicate
and both endpoints ESTABLISHED. Partial and duplicate ACKs cannot initiate close
while data is outstanding. Explicit user-requested close retains its existing API.

## Plan and Acceptance

Add the predicate and use it only in automatic-close scheduling. Regression tests
must generate a two-segment workload, lose its tail, process the first ACK and
assert no close request while the tail is outstanding. Restore delivery, process
RTO recovery, and assert the receiver's cumulative byte progress covers the full
workload before FIN. Cover generation completion and acknowledgement separately.
Build, run focused and full tests, and execute a headless smoke run. Commit the
specification, code/tests and final validation record separately.
