# KNS — Operations

## 1. Purpose

This document describes routine operational procedures for building, testing, and validating KNS.

---

## 2. Repository Verification

Before beginning implementation work:

```text
Check branch
 ↓
Check working tree
 ↓
Inspect current repository
 ↓
Inspect relevant documentation
```

The current target branch is the source of truth.

---

## 3. Build

The build process should use the project's configured CMake workflow and the intended compiler toolchain.

On Windows, the intended toolchain is MinGW at:

```text
C:\mingw64
```

Do not mix runtime files from another compiler environment.

---

## 4. Test

After building:

```text
Run relevant unit tests
        ↓
Run integration tests
        ↓
Run headless scenarios
```

GUI validation should be performed when GUI behavior is affected.

---

## 5. Headless Operation

Headless execution should be used whenever a behavior can be validated without rendering.

This is particularly useful for:

* routing;
* packet delivery;
* TCP;
* event processing;
* simulation lifecycle;
* regression scenarios.

---

## 6. Simulation Diagnostics

When diagnosing simulation behavior, inspect:

* current simulation state;
* logical simulation time;
* pending events;
* topology state;
* link state;
* packet path;
* TCP state.

Avoid diagnosing a failure from GUI appearance alone.

---

## 7. Topology Diagnostics

When a packet fails to reach a destination, inspect:

```text
Source
 ↓
Destination
 ↓
Route
 ↓
Next hop
 ↓
Link state
 ↓
Queue
 ↓
Transmission event
```

---

## 8. TCP Diagnostics

For TCP failures, inspect the sequence:

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

The exact state transitions must match the current implementation.

---

## 9. Issue Diagnostics

When investigating an issue:

1. Reproduce the reported behavior.
2. Inspect the current implementation.
3. Determine whether the issue remains valid.
4. Identify the smallest affected component.
5. Add or identify regression coverage.
6. Implement the correction.
7. Re-run validation.

---

## 10. Clean Environment

When diagnosing build-specific failures, perform a clean configuration using the intended toolchain.

Do not rely on manually copying DLLs from unrelated installations as a permanent fix.

---

## 11. Operational Principle

Prefer reproducible procedures over machine-specific workarounds.

If a fix only works because of a particular PATH configuration, IDE state, or local installation, investigate the underlying environment before considering the problem resolved.
