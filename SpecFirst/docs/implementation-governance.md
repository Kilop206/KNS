# KNS — Implementation Governance

## 1. Purpose

This document defines how changes to KNS should be planned, implemented, validated, and reviewed.

The purpose is to keep implementation aligned with the actual repository, the current specification, and the project's engineering goals.

---

## 2. Source of Truth

For repository-related work, the current GitHub branch being worked on is the primary source of truth unless the user explicitly provides local code for inspection.

For the `tcp` development line, the current `tcp` branch must be inspected before making assumptions about implementation status.

Previous conversations, old code snippets, and historical documentation must not override the current repository.

---

## 3. Specification Before Implementation

Non-trivial changes should follow:

```text
Requirement
    ↓
Specification
    ↓
Acceptance criteria
    ↓
Implementation
    ↓
Tests
    ↓
Review
```

The specification must describe intended observable behavior.

It must not claim that behavior already exists when it has only been planned.

---

## 4. Issue-Driven Changes

An issue must be compared against the current implementation before work begins.

Each issue should be classified as:

```text
Resolved
Partially resolved
Still valid
Obsolete
```

An open issue is not, by itself, evidence that implementation is still required.

---

## 5. Change Scope

Every implementation should define its intended scope.

Prefer the smallest change that completely satisfies the requirement.

Avoid unrelated refactoring unless it is required to safely implement the requested behavior.

---

## 6. Acceptance Criteria

A behavioral change should have observable acceptance criteria.

Example:

```text
Given:
    the simulation is READY

When:
    Start is activated

Then:
    the simulation becomes RUNNING
```

Acceptance criteria should be testable whenever practical.

---

## 7. Compatibility

Before modifying an existing API, evaluate:

* callers;
* tests;
* serialization;
* GUI dependencies;
* headless execution;
* protocol behavior.

Breaking changes must be intentional and documented.

---

## 8. Review Requirements

Before considering a change complete:

* inspect the implementation;
* inspect the diff;
* run relevant tests;
* verify acceptance criteria;
* verify documentation;
* identify unintended side effects.

---

## 9. Documentation Consistency

When behavior changes, all affected specifications must be updated.

Documentation must not contain contradictory descriptions of the same behavior.

---

## 10. AI-Assisted Development

AI may assist with:

* repository analysis;
* design proposals;
* implementation;
* tests;
* documentation;
* issue analysis.

AI-generated assumptions must be validated against the current repository.

AI must not invent repository state, test results, or implementation details.

---

## 11. Completion

A change is complete when:

```text
Requirement satisfied
        +
Tests validated
        +
Documentation consistent
        +
Diff reviewed
```

A successful compilation alone is not sufficient.
