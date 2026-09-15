# Bounded TCP close recovery

Issue #126 remains valid. Each transmitted FIN starts an endpoint-specific timer
even when its first transmission is dropped. Retry the same FIN sequence every
one simulated second, at most five retries. An acknowledged FIN cancels recovery
through state checks. Exhaustion closes both endpoints with explicit
CloseRetriesExhausted status and releases listener occupancy.

Duplicate FIN in closing/TIME_WAIT states is acknowledged without consuming its
sequence again. FIN+ACK processes its valid ACK before peer FIN transition.
TIME_WAIT retention is seven simulated seconds, exceeding the full retry horizon,
so loss of the final ACK can be recovered. CLOSING accepts its final ACK and
transitions to TIME_WAIT. Stale timers and canceled sessions are harmless.

Acceptance: independently lose the active FIN, passive FIN, and final ACK and
recover to CLOSED without failure; permanent loss is bounded, terminal and frees
backlog. Preserve no-loss close behavior. Full CTest and headless tests required.
