# Peer-advertised TCP window

Issue #144 remains valid. Peer window is independent from the local send limit
and congestion window. New DATA must fit all three limits including bytes in
flight. A zero peer window pauses generation without polling; an acceptable
window update resumes the existing generator through ACK processing.

Only accepted SYN/SYN-ACK and in-range ACK-bearing segments update peer state.
Pure window updates must have the expected receive sequence to reject stale
updates. Outgoing DATA and retransmissions advertise local available receive
capacity, not the local send limit. Existing control builders retain the same
capacity advertisement (RST is a special zero-window response).

Plan: expose independent peer window and configurable receive capacity, wire
accepted packet paths, and test zero/reopen, invalid updates, flight bounds and
outgoing advertisements. Full CTest and headless validation are required.
