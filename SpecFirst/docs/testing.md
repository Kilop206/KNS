# KNS — Testing Strategy

## 1. Purpose

Testing verifies that KNS behaves according to its defined contracts.

The primary objective is not maximum line coverage.

The objective is confidence in:

* simulation behavior;
* topology integrity;
* routing;
* packet transmission;
* TCP state transitions;
* loaders;
* GUI/core boundaries;
* headless execution.

---

## 2. Testing Principles

Tests should be:

* deterministic;
* isolated where practical;
* repeatable;
* meaningful;
* focused on observable behavior.

A passing test suite must provide evidence that the system behaves correctly, not merely that code executed without crashing.

---

## 3. Test Levels

KNS uses multiple levels of testing.

```text
Unit tests
    ↓
Component interaction
    ↓
Integration tests
    ↓
Headless simulation scenarios
```

The appropriate level depends on the behavior being tested.

---

## 4. Unit Tests

Unit tests should validate individual component contracts.

Examples include:

* topology validation;
* node management;
* link state;
* queue behavior;
* routing;
* packet behavior;
* TCP state transitions;
* loader validation.

Unit tests should avoid requiring the GUI.

---

## 5. Topology Tests

Topology tests should verify:

* valid node creation;
* invalid node operations;
* link creation;
* link removal;
* duplicate or invalid links where relevant;
* node/link lookup;
* topology consistency.

When topology APIs expose failure behavior, tests must verify the failure contract.

---

## 6. Link Tests

Link tests should verify:

* `UP` behavior;
* `DOWN` behavior;
* transmission acceptance;
* transmission rejection;
* queue behavior;
* capacity limits;
* FIFO ordering where required.

A `DOWN` link must not be accepted as a normal transmission path.

---

## 7. Routing Tests

Routing tests should verify:

* reachable destinations;
* unreachable destinations;
* valid next hops;
* invalid node identifiers;
* link-state awareness;
* behavior after topology changes.

A route should not normally traverse a `DOWN` link.

---

## 8. Packet Tests

Packet tests should verify:

* source and destination;
* protocol information;
* identity;
* transmission context;
* delivery;
* rejection conditions.

Special attention should be given to packets that remain in flight while topology changes.

---

## 9. Simulation Tests

Simulation tests should verify lifecycle behavior:

```text
READY
 ↓ Start
RUNNING
 ↓ Pause
PAUSED
 ↓ Resume
RUNNING
 ↓ queue exhausted
FINISHED
```

Tests must specifically distinguish:

```text
READY + empty queue
```

from:

```text
FINISHED + empty queue
```

This prevents the simulation from being considered complete before it has started.

---

## 10. Event Tests

Event tests should verify:

* scheduling;
* ordering;
* execution;
* logical time;
* queue exhaustion;
* deterministic behavior.

Events with the same logical timestamp must follow the ordering semantics defined by the implementation.

---

## 11. TCP Tests

TCP tests should verify the simplified protocol lifecycle.

At minimum, test:

```text
SYN
 ↓
SYN-ACK
 ↓
ACK
 ↓
Established
```

and:

```text
DATA
 ↓
ACK
```

and:

```text
FIN
 ↓
TIME_WAIT
 ↓
expiration
```

Tests should also cover invalid or failed transitions where those are part of the API contract.

---

## 12. TCP Failure Tests

TCP tests must not only validate successful transitions.

They should also verify behavior when:

* packets are rejected;
* routes are unavailable;
* links go down;
* endpoints become invalid;
* state transitions are invalid.

Failures must not silently become successful protocol transitions.

---

## 13. Loader Tests

Topology loading tests should verify:

* valid input;
* missing required fields;
* malformed values;
* invalid references;
* invalid topology relationships.

Invalid external data must not produce an apparently valid runtime topology.

---

## 14. Headless Testing

Headless execution provides a way to validate the simulation without GUI dependencies.

Headless tests are particularly useful for:

* deterministic scenarios;
* TCP handshake;
* packet transmission;
* routing;
* simulation completion;
* regression testing.

---

## 15. GUI Testing

GUI tests should focus on user-visible behavior and integration with the core.

Examples include:

* simulation controls;
* state display;
* topology interaction;
* connection creation;
* configuration behavior.

GUI tests must verify that:

```text
Create connection
```

does not implicitly mean:

```text
Start simulation
```

when the intended behavior is to remain paused.

---

## 16. Test Discovery

Tests should be discoverable through the project's configured CTest infrastructure.

Where supported, individual tests should be registered so they can be executed independently.

The repository currently uses test discovery mechanisms including:

```text
catch_discover_tests
```

when applicable.

---

## 17. Regression Tests

Every fixed bug that can be represented as a stable automated behavior should receive a regression test.

The regression test should fail under the old behavior and pass under the corrected behavior.

---

## 18. Determinism Tests

Where behavior depends on event ordering or logical time, tests should verify reproducibility.

Equivalent initial conditions should produce equivalent results.

---

## 19. Test Naming

Test names should describe behavior.

Prefer:

```text
link_down_rejects_transmission
```

over:

```text
test_link_2
```

A test name should communicate what contract is being verified.

---

## 20. Validation Before Commit

Before committing a behavioral change:

```text
Build
 ↓
Run relevant tests
 ↓
Run broader test suite
 ↓
Inspect failures
 ↓
Inspect git diff
 ↓
Commit
```

Do not claim validation that was not actually performed.

---

## 21. Test Maintenance

Tests must evolve with the implementation.

When an API contract changes:

* update affected tests;
* add coverage for the new behavior;
* remove obsolete expectations;
* update documentation.

A stale test is not evidence of correct behavior.

---

## 22. Testing Completion Criteria

A change is sufficiently validated when:

* relevant tests pass;
* regression coverage exists where appropriate;
* headless behavior remains correct;
* no unrelated tests regress;
* the changed contract is documented.
