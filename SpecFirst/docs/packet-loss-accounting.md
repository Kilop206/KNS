# Packet Loss Accounting - Issue #128

Still valid at `aa8865c`: a forwarding event adds a loss after sendPacket already
counted queue rejection or a random drop.

## Required Contract and Plan

Each rejected topology transmission counts exactly once regardless of hop.
The engine owns queue-full and random-drop accounting. PacketUtils owns failure
to resolve a route or usable link before calling the engine. Callers, including
PacketReceivedEvent, propagate the result without adding another loss. Existing
direct-send API misuse (foreign/removed link) retains its no-side-effects contract.

Remove the forwarding increment and account for topology-resolution failure in
PacketUtils. Test first-hop and intermediate-hop queue full, probability-one
drop, DOWN link and disconnected route, asserting a delta of exactly one loss.
After focused regression tests, run full CTest, build and headless validation.
