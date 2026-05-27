# Event Engine Design

## 1. Purpose

The Event Engine is the core of KNS.  
It implements a discrete-event simulation model focused on determinism and reproducibility.

---

## 2. Simulation Time Model

- Time type: int64_t
- Unit: logical ticks
- Time is logical and does not depend on the system clock.
- Time advances only when an event is processed.

**Rationale:**

Using int64_t ensures a large time range suitable for long-running simulations.  
The logical time model provides full control over time progression, eliminating external dependencies and ensuring determinism.

---

## 3. Event Definition

An event is defined as:

- Timestamp (logical time)
- Unique incremental identifier
- Action to be executed

**Responsibility:**

An event represents a scheduled action to occur at a specific simulation time.

---

## 4. Event Ordering Policy

Events are ordered according to the following rules:

1. Lower timestamp first
2. In case of ties, lower ID first (insertion order)

This policy guarantees absolute determinism.

---

## 5. Execution Model

The engine operates as follows:

1. While the queue is not empty:
    - Remove the next event
    - Update the current simulation time
    - Execute the associated action

The engine can:

- Run until the queue is empty
- Run until a time limit is reached
- Be paused and resumed

---

## 6. Determinism Guarantee

Given the same initial set of events and the same insertion order:

- Execution will always produce the same event order
- The final state will always be identical

---

## 7. Known Limitations (Initial)

- Single-threaded execution
- No parallelism in the core
- No dependency on real time