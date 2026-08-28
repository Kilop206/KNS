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

## 6. Routing vs. Transmission

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

## 7. Regression Coverage

A corrected issue should receive regression coverage whenever the behavior can be reliably tested.

Regression tests should verify observable behavior rather than implementation details whenever practical.

---

## 8. Issue Completion

An issue should only be considered complete after:

* the implementation satisfies the requirement;
* acceptance criteria are satisfied;
* relevant tests pass;
* no unintended regression is identified;
* affected documentation is consistent.

---

## 9. Current Repository as Authority

Issue status must always be evaluated against the current repository.

Historical implementations, previous conversations, and outdated documentation must not be treated as authoritative.
