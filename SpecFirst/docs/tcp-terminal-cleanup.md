# TCP Terminal Cleanup - Issues #118 and #142

## Investigation and Required Contract

Both issues were still valid on local `tcp` at `445920b`.
SYN retry exhaustion returned without a terminal transition. Public
listener cleanup destroyed buffers regardless of connection state.

Implemented observable behavior:

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

## Completion Record - 2026-09-14

Specification: `80e5fa2`. Terminal cleanup: `ebee95c` (#142).
Handshake exhaustion: `5fded04` (#118).

Validated with the existing MinGW/Ninja C++20 build, 10 focused CTest cases,
and the complete 257-case suite (all passed). The headless `mesh4.json` smoke
run exited successfully and exported 148 sent/delivered packets, zero loss,
zero packets in transit, and four sessions.

The failure reason is available from `TCPSession::getFailureReason()`; it is
not a new CSV field. Failed sessions remain inspectable and are not counted as
successfully completed traffic by simulation validation.
