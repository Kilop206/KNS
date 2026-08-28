# KNS — SpecFirst

## Specification-First Engineering Documentation

This directory contains the **SpecFirst engineering documentation for KNS (Kinetic Network Simulator)**.

SpecFirst is used here as a development discipline for keeping requirements, architecture, implementation, tests, issues, and engineering decisions synchronized.

It is not a second source of truth for the codebase.

The repository and its current development branch remain authoritative for implementation state.

---

## What is KNS?

KNS (Kinetic Network Simulator) is a C++20 discrete-event network simulator.

The project provides a simulation environment for network topologies, links, packet transmission, routing, and a simplified TCP model, with a graphical interface based on:

* Dear ImGui;
* GLFW;
* OpenGL.

KNS also supports headless execution and automated testing.

---

## Purpose of SpecFirst in KNS

SpecFirst exists to answer five questions before and during implementation:

1. **What should KNS do?**
2. **How is that behavior represented architecturally?**
3. **What does the current implementation actually do?**
4. **How do we verify the behavior?**
5. **Why was a particular engineering decision made?**

The documentation therefore connects:

```text
Requirements
    ↓
Specification
    ↓
Architecture
    ↓
Implementation
    ↓
Tests
    ↓
Engineering decisions
```

---

## Source of Truth

For implementation work, the current Git branch is the primary source of truth.

For TCP development:

```text
repository: Kilop206/KNS
branch: tcp
```

Do not assume that local files, previous conversations, issue descriptions, or historical commits represent the current implementation.

When investigating an issue:

```text
Current branch
     ↓
Current code
     ↓
Issue
     ↓
Specification
     ↓
Change
```

---

## Documentation Structure

### Core project definition

* `project-overview.md` — project scope and purpose.
* `architecture.md` — system architecture and boundaries.
* `domains.md` — domain responsibilities.
* `data-model.md` — important domain entities and relationships.
* `workflows.md` — important runtime and development workflows.

### Engineering practices

* `coding-standards.md` — coding conventions.
* `testing.md` — testing strategy.
* `security.md` — security requirements.
* `design-guidelines.md` — GUI design rules.
* `operations.md` — build, execution, and operational procedures.
* `tooling-adapters.md` — tooling-specific integration rules.
* `ai-workflow.md` — AI-assisted engineering workflow.

### Governance

* `implementation-governance.md` — rules for implementing changes.
* `implementation-plan.md` — implementation phases and planned work.
* `issues.md` — issue analysis and implementation tracking.
* `decision-log.md` — significant engineering decisions.
* `deployment-log.md` — relevant delivery and deployment history.

---

## SpecFirst Workflow

The standard workflow is:

```text
Issue / Requirement
        ↓
Inspect current KNS implementation
        ↓
Classify current behavior
        ↓
Define desired behavior
        ↓
Update specification
        ↓
Define acceptance criteria
        ↓
Implement
        ↓
Test
        ↓
Review diff
        ↓
Synchronize documentation
```

An issue being open does not mean that its original problem still exists.

Every issue must first be compared against the current implementation.

---

## Issue Classification

Issues should be classified as one of:

```text
Resolved
Partially resolved
Still valid
Obsolete
```

This prevents historical issues from becoming unnecessary implementation work.

---

## Architecture Principle

The most important architectural boundary is:

```text
GUI / Application
        ↓
     KNS Core
        ↓
Simulation / Network Model
```

The core must remain independent of the graphical interface.

---

## Simulation Principle

Simulation execution is explicitly controlled.

The intended lifecycle is:

```text
READY
  ↓ Start
RUNNING
  ↓ Pause
PAUSED
  ↓ Resume
RUNNING
  ↓ event queue exhausted
FINISHED
```

Creating a network connection is not equivalent to starting simulation execution.

---

## Development Principle

KNS should favor:

* deterministic behavior;
* explicit state;
* clear contracts;
* small changes;
* automated tests;
* reproducible builds;
* documented decisions.

The goal is not merely to make the simulator work, but to make its behavior understandable and maintainable.

---

## Root Engineering Contract

`AGENTS.md` is the root engineering contract for this directory.

All contributors and AI-assisted workflows must follow it.

When a rule is needed only for a specific technical area, it should be documented in the appropriate file under `docs/` rather than duplicated in `AGENTS.md`.
