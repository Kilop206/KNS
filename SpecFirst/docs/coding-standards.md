# KNS — Coding Standards

## 1. Purpose

These standards define the coding practices expected in the KNS C++20 codebase.

The goal is maintainable, deterministic, testable, and readable code.

The existing repository style takes precedence where it is more specific.

---

## 2. Language Standard

KNS targets:

```text
C++20
```

New code should use C++20 features when they improve clarity without unnecessarily increasing complexity.

---

## 3. Naming

Names should communicate intent.

Prefer:

```cpp
calculateNextHop()
```

over ambiguous names such as:

```cpp
doStuff()
```

Types should use clear, descriptive names.

Boolean values should read naturally when used in conditions.

---

## 4. Classes

Classes should have a clear responsibility.

Avoid classes that simultaneously manage:

* GUI rendering;
* simulation scheduling;
* topology manipulation;
* protocol logic.

When responsibilities become unrelated, consider separating them.

---

## 5. Functions

Functions should be:

* focused;
* deterministic where possible;
* explicit about failure;
* small enough to understand;
* easy to test.

Avoid functions that silently perform unrelated side effects.

---

## 6. Error Handling

Invalid operations must be observable.

For example, if:

```text
sendPacket()
```

rejects a packet, a higher-level wrapper must not report successful transmission.

Failure semantics should be defined by the API.

Avoid silently ignoring errors unless ignoring them is explicitly part of the contract.

---

## 7. State Machines

State-machine implementations should make valid and invalid transitions explicit.

This is especially important for:

* simulation state;
* TCP state;
* link state.

Do not hide meaningful state transitions inside unrelated helper functions.

---

## 8. Ownership

Ownership should be explicit.

Prefer RAII and standard C++ ownership mechanisms.

Avoid raw owning pointers unless there is a documented reason.

When references or pointers are stored for later use, their lifetime must be clear.

---

## 9. Containers

Choose containers according to their semantic requirements.

Consider:

* ordering;
* lookup complexity;
* ownership;
* invalidation behavior;
* deterministic iteration.

Do not choose a container merely because it is familiar.

---

## 10. Event Scheduling

Event scheduling must preserve deterministic behavior.

Code that schedules events should make clear:

* when the event occurs;
* what entity it affects;
* what state it expects;
* what happens if the relevant entity is no longer valid.

---

## 11. Topology APIs

Topology APIs must have clear contracts.

Methods involving:

* node lookup;
* link lookup;
* link creation;
* link removal;
* next-hop calculation;

must define invalid input and failure behavior.

---

## 12. Link State

Code that transmits or routes packets must respect link availability.

Do not duplicate link-state rules across unrelated layers.

The link's operational state should have one authoritative meaning.

---

## 13. TCP Code

TCP implementation should favor explicit state transitions.

Changes to TCP must consider:

* handshake;
* sequence numbers;
* acknowledgements;
* data;
* FIN;
* TIME_WAIT;
* failure transitions.

Protocol state should not be inferred indirectly from unrelated variables when an explicit state representation is available.

---

## 14. GUI Code

GUI code should be responsible for presentation and user interaction.

Do not place core simulation algorithms inside rendering functions.

A button should invoke a clearly defined core operation rather than reproduce the operation's internal logic.

---

## 15. Comments

Comments should explain:

* why something is necessary;
* an invariant;
* a non-obvious algorithmic choice;
* a compatibility constraint.

Avoid comments that merely restate obvious code.

---

## 16. Constants

Avoid unexplained magic numbers.

Prefer named constants when a value represents:

* timeout;
* packet size;
* simulation interval;
* capacity;
* protocol parameter.

---

## 17. Determinism

Avoid uncontrolled sources of nondeterminism in simulation behavior.

When ordering matters, make the ordering explicit.

Do not rely on incidental container ordering when simulation correctness depends on it.

---

## 18. Tests

New behavior should be accompanied by tests.

Tests should validate externally observable contracts.

Avoid tests that merely duplicate the implementation.

---

## 19. Refactoring

Do not combine broad refactoring with an unrelated bug fix unless necessary.

Prefer:

```text
Bug fix
+
Minimal supporting refactor
```

over:

```text
Bug fix
+
Unrelated architectural rewrite
```

---

## 20. Formatting

Follow the formatting already established by the repository and build tooling.

Do not introduce a second formatting style into an existing source file.

Consistency is more important than personal preference.

---

## 21. Dependency Discipline

Avoid introducing dependencies without a clear project need.

Core dependencies should remain independent of GUI dependencies.

---

## 22. Review Checklist

Before considering code complete, verify:

* [ ] Responsibility is clear.
* [ ] Failure behavior is explicit.
* [ ] Ownership is understood.
* [ ] State transitions are valid.
* [ ] Determinism is preserved.
* [ ] Existing APIs remain compatible where required.
* [ ] Tests cover the changed behavior.
* [ ] No unrelated code was modified.
