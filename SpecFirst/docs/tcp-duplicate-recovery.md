# TCP Duplicate Recovery

Issues #131 and #123 were still valid on `tcp` at `82b004d`.
The contracts below are now implemented locally.

## Required Behavior

* A duplicate SYN in SYN_RECEIVED is accepted only if its sequence matches the
  originally accepted SYN. It leaves sequence numbers and receive state intact
  and causes the same SYN-ACK to be sent again. Endpoint/session dispatch and
  listener capacity checks remain in force.
* An established receiver immediately acknowledges a nonempty retransmitted DATA
  range entirely below its cumulative receive boundary. Duplicate bytes are not
  inserted or consumed again; the cumulative ACK and receive occupancy do not
  change. Empty, partially overlapping and invalid-state data must not be newly
  accepted by this rule. Sequence wraparound remains tracked separately in #138.
* Duplicate DATA ACKs cancel pending delayed ACKs using the existing mechanism.

## Plan and Acceptance

1. Add sequence-matched duplicate SYN handling, test mismatched SYN rejection and
   recover an intentionally lost SYN-ACK through the actual handshake timer.
2. Add duplicate DATA classification before receive-buffer insertion and reuse
   immediate ACK handling. Test lost ACK recovery, unchanged receive state,
   sender buffer acknowledgement, and invalid/empty/overlapping input.
3. Commit each fix independently; run focused tests, full CTest, build and a
   headless smoke scenario. Record results before marking local completion.

## Completion Record

Specification: `ceee572`. Duplicate SYN: `04545e0` (#131).
Duplicate DATA ACK: `dfd69e8` (#123).

The full MinGW/Ninja C++20 build succeeded in `build/validation-bin`.
All 266 CTest cases passed, including four new regression cases. The mesh4
headless run exited successfully with 148 sent/delivered packets, 80 DATA,
zero losses and zero packets in transit. The targeted tests separately force
SYN-ACK and ACK loss and assert recovery through scheduled events.

Commits remain local; GitHub issues remain open pending publication.
