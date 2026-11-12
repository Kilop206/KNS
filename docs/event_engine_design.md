# Event Engine Design

## Time model

KNS uses discrete-event simulation. `SimulationClock` stores a `double` value in
simulated seconds. Dispatch moves the clock to the event's timestamp; a bounded
run can also advance to its horizon without skipping a pending event. The core never sleeps or
uses wall-clock time to decide protocol behavior.

The GUI may pace rendering against real time, but that presentation concern does
not change the logical schedule.

## Event contract

Every event derives from `Event` and provides:

- a simulation timestamp;
- a monotonic 64-bit ID assigned at construction;
- `execute(SimulationEngine&)`;
- an optional human-readable name for diagnostics.

Timestamps express when an event becomes eligible. IDs are the deterministic
tie-breaker and do not represent protocol session IDs.

## Queue ordering and ownership

`EventQueue` owns pending events as `std::unique_ptr<Event>` in a priority queue.
Its comparator orders:

1. lower timestamp first;
2. lower event ID first when timestamps are equal.

Scheduling a null pointer throws `std::invalid_argument`. `next()` transfers
ownership of the earliest event to the caller and returns `nullptr` for an empty
queue. `peekTimestamp()` returns positive infinity when no event is pending.

## Engine execution APIs

`SimulationEngine::run()` repeatedly:

1. removes the next event;
2. moves the logical clock to its timestamp;
3. executes it;
4. continues until the queue is empty.

`processEvent()` performs one iteration and is used by the interactive
application. `hasEvents()` and `peekNextEventTime()` support external control.
The core also supports cooperative pause/resume, explicit stepping, terminal
cancellation and bounded execution. See [simulation-model.md](simulation-model.md)
for exact horizon, budget, failure and pending-work semantics.

## Cancellation and stale events

`cancelEvent(id)` removes a pending event without advancing the clock. In addition,
events carry stable IDs such as a TCP session ID and validate current
state when executed. Examples include:

- timeout events returning after their segment was acknowledged;
- delayed ACK events returning after a newer ACK superseded them;
- TCP events returning after their session was cancelled;
- link-arrival cleanup becoming a no-op after link removal.

This approach avoids mutable priority-queue indexing and keeps invalidation
local to the subsystem that understands the event.

## Determinism guarantee

On the same implementation, equal initial state, random seed, input data, and
event insertion order produce the same dispatch order. A callback that reads
wall-clock time, external mutable data, or an uncontrolled random source can
break that guarantee and should not be introduced into core event execution.

## Current constraints

- execution is single-threaded;
- timestamps use floating-point seconds;
- legacy `run()` drains the queue; `runUntil()` supplies a horizon and event budget;
- event IDs are process-wide and monotonic;
- cancellation is cooperative and execution must stay on one owner thread.

These constraints favor a small, predictable engine. Features such as bounded
runs should be added as explicit engine APIs with ordering and ownership tests.
