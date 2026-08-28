# KNS — Issue Management

## 1. Purpose

This document defines how KNS issues should be analyzed and resolved.

---

## 2. Issue Is Not Implementation

An open issue represents a reported concern or requested change.

It does not prove that the problem still exists.

Before implementation, compare the issue against the current branch.

---

## 3. Issue Classification

Each reviewed issue should be assigned one of:

### Resolved

The current implementation already satisfies the requirement.

### Partially Resolved

The implementation addresses part of the issue, but a relevant gap remains.

### Still Valid

The reported problem remains present.

### Obsolete

The issue no longer applies because the architecture, requirement, or affected component has changed.

---

## 4. Investigation Procedure

For each issue:

```text
Issue
 ↓
Current repository
 ↓
Relevant implementation
 ↓
Relevant tests
 ↓
Specification
 ↓
Classification
```

Do not implement an issue before this comparison.

---

## 5. Valid Issue

A valid issue should have:

* a clear problem;
* reproducible behavior where applicable;
* affected component;
* expected behavior;
* acceptance criteria.

---

## 6. Duplicate Issues

If multiple issues describe the same underlying defect, consolidate their implementation work where appropriate.

Do not create multiple independent fixes for one root cause.

---

## 7. Regression Coverage

A corrected issue should receive regression coverage whenever the behavior can be tested reliably.

---

## 8. Issue Completion

An issue should only be considered complete after:

* implementation is complete;
* relevant tests pass;
* acceptance criteria are satisfied;
* documentation is updated where required.

---

## 9. Current Known Areas

Known areas requiring continued engineering attention include:

* packet transmission success propagation;
* packet/link identity during transmission;
* link availability in routing and transmission;
* FIFO queue behavior;
* dynamic topology changes;
* topology API validation;
* `getNextHop()` bounds and failure behavior;
* `getLinksFromNode()` contract;
* aggregate `TCPSession` state;
* `TCPConnection` transition failures;
* simulation lifecycle behavior.

The current issue tracker remains authoritative for issue numbers and status.
