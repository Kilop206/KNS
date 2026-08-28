# KNS — Testing

## 1. Testing Principles

Testing must validate observable KNS behavior and protect important architectural contracts.

Tests should prefer deterministic scenarios and should avoid unnecessary dependence on GUI rendering.

---

## 2. Test Layers

KNS testing may include:

```text
Unit Tests
    ↓
Component Tests
    ↓
Integration Tests
    ↓
Headless Simulation Tests
    ↓
GUI Validation
```

Not every change requires every layer.

---

## 3. Core Testing

Core simulation behavior should be testable without the GUI.

Relevant areas include:

* event scheduling;
* logical time;
* topology;
* links;
* packet transmission;
* routing;
* TCP state transitions.

---

## 4. Link Tests

Link tests should verify:

* connection endpoints;
* operational state;
* queue behavior;
* queue capacity;
* transmission behavior;
* invalid operations.

The existence of a queue must not be treated as proof that all FIFO semantics are already validated.

---

## 5. Routing Tests

Routing tests should verify:

* valid paths;
* unreachable destinations;
* invalid nodes;
* shortest-path behavior;
* behavior when links are `DOWN`.

The current routing implementation excludes `DOWN` links.

Transmission through `DOWN` links is a separate contract and must be tested independently.

---

## 6. TCP Tests

TCP tests should cover, where applicable:

```text
SYN
 ↓
SYN-ACK
 ↓
ACK
 ↓
Established
 ↓
DATA / ACK
 ↓
FIN
 ↓
TIME_WAIT
```

Tests should also cover:

* invalid transitions;
* retry behavior;
* timeout behavior;
* sequence and acknowledgement handling;
* termination;
* TIME_WAIT expiration.

---

## 7. Simulation Lifecycle Tests

The simulation lifecycle is explicit:

```text
READY
  ↓ Start
RUNNING
  ↓ Pause
PAUSED
  ↓ Resume
RUNNING
  ↓ event processing completes
FINISHED
```

An empty event queue before execution must not be interpreted as `FINISHED`.

Loading a topology and creating a connection must remain distinguishable from starting simulation execution.

---

## 8. GUI Tests

GUI validation should verify user-visible behavior when GUI code is affected.

Examples include:

* simulation controls;
* state display;
* topology interaction;
* node manipulation;
* configuration controls.

Core behavior should not depend exclusively on GUI tests.

---

## 9. Headless Tests

Headless execution should be preferred for automated validation when rendering is unnecessary.

This is particularly appropriate for:

* routing;
* packet delivery;
* TCP;
* event processing;
* simulation lifecycle;
* regression scenarios.

---

## 10. Test Discovery

Where configured, tests should be discoverable through the project's CTest integration.

The repository may use Catch2 test discovery through `catch_discover_tests`.

---

## 11. Regression Tests

Bug fixes should add regression coverage when practical.

A regression test should reproduce the relevant failure before the fix and verify the expected behavior after the fix.

---

## 12. Test Status

Documentation must distinguish:

### Implemented

Behavior currently verified by existing tests.

### Required

Behavior that must have test coverage but may not yet be fully covered.

### Planned

Future testing work that has not yet been implemented.

Documentation must not claim that a test exists unless the repository actually contains and registers it.
