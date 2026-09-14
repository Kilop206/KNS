# TCP Terminal Cleanup - Issues #118 and #142

## Investigation and Required Contract

Both issues remain valid on local `tcp` at `445920b`.
SYN retry exhaustion currently returns without a terminal transition. Public
listener cleanup currently destroys buffers regardless of connection state.

Required observable behavior:

* `releaseTCPListenerSession` returns false for missing sessions or when either
  endpoint is not CLOSED, without changing buffers or listener occupancy.
* Successful terminal cleanup clears transport buffers and untracks the listener.
  Repeated terminal cleanup is harmless and returns true.
* Explicit cancellation untracks the listener and erases the session at any state.
* Exhausting SYN retries closes both endpoints, retains the session with the
  typed failure reason `SynRetriesExhausted`, releases listener capacity, and
  schedules no further handshake retry. Stale timeout events are harmless.

## Implementation and Validation Plan

1. Separate listener bookkeeping from terminal buffer cleanup; test active,
   partially closed, CLOSED and missing sessions plus existing cancellation.
2. Add the session failure reason and exhaustion transition using the existing
   connection failure operation; test exhaustion directly and under permanent loss.
3. Build, run focused regressions and the full CTest suite, then a headless smoke
   run. Commit each implementation step independently and record actual results.
