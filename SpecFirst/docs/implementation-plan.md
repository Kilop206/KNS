# KNS — Implementation Plan

## 1. Purpose

This document defines the general implementation process used to evolve KNS.

It is a process document rather than a permanent list of individual tasks.

---

## 2. Phase 1 — Understand

Before modifying code:

1. Inspect the current branch.
2. Inspect applicable repository instructions.
3. Inspect the relevant specification.
4. Identify affected components.
5. Identify existing tests.
6. Check related issues.

---

## 3. Phase 2 — Specify

Define:

* problem;
* desired behavior;
* constraints;
* acceptance criteria;
* affected domains;
* expected failure behavior.

For complex changes, update the relevant SpecFirst documents before implementation.

---

## 4. Phase 3 — Design

Determine:

* affected APIs;
* state transitions;
* data flow;
* event behavior;
* ownership;
* compatibility concerns;
* test strategy.

Prefer designs that preserve existing architecture unless a structural change is justified.

---

## 5. Phase 4 — Implement

Implementation should be incremental.

Prefer:

```text
Small change
 ↓
Build
 ↓
Test
 ↓
Inspect
 ↓
Next change
```

rather than accumulating many unrelated modifications.

---

## 6. Phase 5 — Validate

Validation should include the smallest relevant test set first.

Then run broader validation when practical.

Validation should include:

* compilation;
* unit tests;
* integration tests;
* headless scenarios;
* GUI validation when applicable.

---

## 7. Phase 6 — Review

Review:

```text
Code
Tests
Specification
Diff
Issue
```

Check for:

* accidental changes;
* undocumented behavior;
* inconsistent contracts;
* unnecessary complexity.

---

## 8. Phase 7 — Integrate

After validation:

1. Stage intended changes.
2. Review staged diff.
3. Commit with a meaningful message.
4. Push to the appropriate branch.

---

## 9. Issue Resolution Order

Issues should generally be prioritized by:

1. correctness;
2. data integrity;
3. simulation determinism;
4. core architecture;
5. protocol correctness;
6. testing infrastructure;
7. GUI behavior;
8. documentation and polish.

Dependencies between issues may change this order.

---

## 10. Reassessment

Implementation plans are not immutable.

If repository state changes, reassess the plan before continuing.

The current implementation always takes precedence over assumptions made when the plan was written.
