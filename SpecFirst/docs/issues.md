# KNS — Issue Management

## 1. Purpose

This document defines how KNS issues are investigated, classified, implemented, and closed.

---

## 2. Issue Is Not Implementation

An open issue represents a reported concern or requested change.

It does not prove that the problem still exists.

Every issue must be compared against the current target branch before implementation.

---

## 3. Issue Classification

Each reviewed issue must be classified as one of:

### Resolved

The current implementation satisfies the issue's requirement.

### Partially Resolved

The implementation addresses part of the issue, but a relevant gap remains.

### Still Valid

The reported problem remains present.

### Obsolete

The issue no longer applies because the architecture, requirement, or implementation has changed.

---

## 4. Investigation Procedure

For each issue:

```text
Issue
 ↓
Current Repository
 ↓
Relevant Implementation
 ↓
Relevant Tests
 ↓
Specification
 ↓
Classification
```

Implementation must not begin solely from the issue description.

---

## 5. Current Known Engineering Areas

The following areas have been identified for investigation or continued development.

Their presence here does not imply that every item is currently unresolved.

### Packet transmission

Verify that higher-level transmission functions correctly propagate failures from lower-level packet transmission operations.

### In-flight packet and link identity

Verify that a packet being transmitted retains an unambiguous association with the link responsible for the transmission.

### Link operational state

Routing already excludes links whose operational state is `DOWN`.

Transmission behavior must independently ensure that unavailable links cannot be used for invalid packet transmission.

### Queue semantics

Verify that link transmission queues satisfy the intended FIFO and capacity contracts.

### Dynamic topology changes

Define and validate the behavior of already-scheduled events and in-flight packets when topology elements are modified or removed.

### Topology API validation

Validate identifiers, references, and invalid operations exposed through the `Topology` API.

### Routing bounds and contracts

Validate `getNextHop()` bounds and failure behavior.

Validate the contract and returned representation of `getLinksFromNode()`.

### TCP session state

Review the aggregate state represented by `TCPSession` and its relationship with the contained client and server `TCPConnection` instances.

### TCP transition failures

Ensure failed or invalid TCP state transitions are observable and handled consistently.

### Simulation lifecycle

Ensure that simulation lifecycle state is explicitly distinguished from the presence or absence of queued events.

---

## 6. Resolved Issue Records

### Issue #81 — Configurable routing metrics and explicit table rebuild

**Classification:** Partially resolved at investigation; resolved by commits
`53ebba2` and `9c1cdfb`.

The routing core already supported delay, bandwidth, hop-count, and
delay-bandwidth metrics, and it rebuilt tables for engine-owned topology
changes. The remaining gap was observable behavior: the GUI node panel rebuilt
its own delay-only table, so it could disagree with the engine after a metric
selection or topology edit.

**Acceptance criteria met:**

* the GUI selects one of the supported metrics through `SimulationEngine`;
* node details read the engine's current routing table;
* metric changes and topology updates through `SimulationEngine` or the GUI rebuild the active tables;
* invalid table sources return an empty read-only view.

Regression coverage is in `tests/network/RoutingMetricTests.cpp`. Full CTest
validation and a headless simulation completed successfully on 2026-09-11.

### Issue #102 — Link parameter validation

**Classification:** Still valid at investigation; resolved by commits
`f34bbcc` and `69b6738`.

`Link` now admits only finite transmission parameters: positive bandwidth,
non-negative delay, and loss probability in `[0, 1]`. Invalid constructor and
setter calls raise `std::invalid_argument`; validation occurs before assignment,
so rejected setters preserve the previous value. `Topology` applies the same
finite-value checks at its public creation and global-loss entry points.

**Acceptance criteria met:**

* constructor and setters reject invalid finite and non-finite values;
* bandwidth, delay, and loss-probability bounds are enforced;
* failed setter calls leave link state unchanged;
* topology entry points reject non-finite values;
* regression tests cover zero, negative, `NaN`, and infinity inputs.

Regression coverage is in `tests/network/LinkTests.cpp`. A full CMake build,
225 passing CTest cases, and a headless `mesh4.json` simulation completed on
2026-09-11.

### Issue #103 — Removed nodes cannot be reconnected implicitly

**Classification:** Still valid at investigation; resolved by commits
`2071cda` and `9253789`.

`Topology::addLinkPtr()` now rejects inactive existing endpoints with
`std::invalid_argument`. This preserves removed-node identity without allowing
implicit reactivation; IDs beyond topology size retain automatic creation.

Regression coverage in `tests/network/DynamicTopologyTests.cpp` verifies that
the link set and routing stay unchanged. A full build and 226 passing CTest
cases completed on 2026-09-11.

---

## 7. Routing vs. Transmission

These are separate concerns.

The current routing implementation already excludes `DOWN` links.

Therefore:

```text
Routing
    ↓
DOWN link excluded
```

does not imply:

```text
Transmission
    ↓
DOWN link behavior completely validated
```

Both behaviors must be tested independently.

---

## 8. Regression Coverage

A corrected issue should receive regression coverage whenever the behavior can be reliably tested.

Regression tests should verify observable behavior rather than implementation details whenever practical.

---

## 9. Issue Completion

An issue should only be considered complete after:

* the implementation satisfies the requirement;
* acceptance criteria are satisfied;
* relevant tests pass;
* no unintended regression is identified;
* affected documentation is consistent.

---

## 10. Current Repository as Authority

Issue status must always be evaluated against the current repository.

Historical implementations, previous conversations, and outdated documentation must not be treated as authoritative.
