# TCP Duplicate Recovery

Issues #131 and #123 remain valid on `tcp` at `82b004d`.

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
