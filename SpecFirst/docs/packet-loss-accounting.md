# Packet Loss Accounting - Issue #128

Still valid at investigation on `aa8865c`: a forwarding event added a loss after
sendPacket already counted queue rejection or a random drop. Corrected locally.

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

## Completion

Specification `6676ad8`; implementation `7737120`. Eight combinations of failure
type and hop position pass. All 269 CTest cases pass after the MinGW/Ninja build.
The mesh4 headless run in `build/validation-bin` exits successfully with 148
delivered packets, 80 DATA, zero losses and zero packets in transit.
Commits remain local and GitHub issues remain open pending publication.
