# KNS — Decision Log

## 1. Purpose

This document records significant architectural and behavioral decisions affecting KNS.

Decisions should explain why a direction was chosen, not merely what code changed.

---

## Decision 001 — Discrete-Event Simulation

**Status:** Accepted

KNS uses a discrete-event simulation model with deterministic logical time.

The simulator processes scheduled events rather than directly coupling simulation progression to wall-clock time.

---

## Decision 002 — Headless Execution

**Status:** Accepted

Core simulation behavior must be executable without requiring the GUI.

This enables automated testing and deterministic simulation scenarios.

---

## Decision 003 — Simulation Lifecycle States

**Status:** Accepted

The simulation lifecycle distinguishes:

```text
Ready
Running
Paused
Finished
```

An empty event queue before execution must not be interpreted as a completed simulation.

---

## Decision 004 — Explicit Simulation Start

**Status:** Accepted

Creating or scheduling a network connection must not implicitly start simulation execution.

Network configuration and simulation execution are separate responsibilities.

---

## Decision 005 — Link Operational State

**Status:** Accepted

Links have an operational state represented conceptually as:

```text
UP
DOWN
```

Routing and normal transmission must respect link availability.

---

## Decision 006 — Simplified TCP

**Status:** Accepted

KNS implements a simplified TCP model for simulation purposes rather than attempting to reproduce every aspect of production TCP.

The implementation nevertheless models meaningful connection states, control exchange, data transfer, acknowledgement, termination, and TIME_WAIT behavior.

---

## Decision 007 — Current Repository as Source of Truth

**Status:** Accepted

Implementation analysis must be based on the current repository state and target branch.

Historical snippets and previous implementation assumptions must not override the current code.

---

## Decision 008 — Specification-First Development

**Status:** Accepted

Non-trivial behavior changes should be specified before implementation.

The specification and implementation must remain consistent.

---

## Decision 009 — Issue Revalidation

**Status:** Accepted

Open issues must be revalidated against the current implementation before being implemented.

An issue may be resolved, partially resolved, still valid, or obsolete.

---

## Decision 010 — GUI/Core Separation

**Status:** Accepted

The GUI is responsible for presentation and user interaction.

Core simulation behavior should remain independent from GUI rendering.

---

## Decision 011 — MinGW Toolchain

**Status:** Accepted

The intended Windows development toolchain is MinGW installed at:

```text
C:\mingw64
```

The project should not mix incompatible compiler/runtime environments.

In particular, MSYS2 UCRT64 artifacts should not be mixed with the intended MinGW environment.

---

## Decision 012 — Documentation Language

**Status:** Accepted

The SpecFirst engineering documentation is maintained in English.

The documentation should use KNS-specific terminology and avoid generic templates that do not correspond to the project.
