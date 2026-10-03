# KNS Documentation

This directory contains the engineering documentation used to specify, implement, test, and maintain the KNS (Kinetic Network Simulator).

The documentation is organized around the actual architecture and development workflow of KNS rather than around a generic project template.

---

## Documentation Map

### Project

| Document              | Purpose                                                      |
| --------------------- | ------------------------------------------------------------ |
| `project-overview.md` | Defines KNS, its scope, goals, and major capabilities        |
| `architecture.md`     | Defines system boundaries and architectural responsibilities |
| `domains.md`          | Describes the major KNS domains                              |
| `data-model.md`       | Describes important domain entities and relationships        |
| `workflows.md`        | Describes important simulation and development workflows     |

### Engineering

| Document               | Purpose                                          |
| ---------------------- | ------------------------------------------------ |
| `coding-standards.md`  | C++ and project coding conventions               |
| `testing.md`           | Test strategy and validation requirements        |
| `security.md`          | Security and credential-handling requirements    |
| `design-guidelines.md` | GUI visual and interaction guidelines            |
| `operations.md`        | Build, execution, and troubleshooting procedures |
| `tooling-adapters.md`  | Tool-specific project integration                |
| `ai-workflow.md`       | AI-assisted engineering workflow                 |

### Governance

| Document                       | Purpose                                        |
| ------------------------------ | ---------------------------------------------- |
| `implementation-governance.md` | Rules governing implementation work            |
| `implementation-plan.md`       | Planned implementation phases                  |
| `issues.md`                    | Analysis and tracking of implementation issues |
| `decision-log.md`              | Significant engineering decisions              |
| `deployment-log.md`            | Delivery and deployment history                |

---

## How to Use the Documentation

Documentation should be read according to the task.

### Architectural work

Start with:

```text
project-overview.md
        ↓
architecture.md
        ↓
domains.md
        ↓
data-model.md
```

### Feature or bug implementation

Start with:

```text
Issue
 ↓
project-overview.md
 ↓
architecture.md
 ↓
relevant domain document
 ↓
workflows.md
 ↓
implementation-governance.md
 ↓
testing.md
```

### GUI work

Start with:

```text
architecture.md
 ↓
workflows.md
 ↓
design-guidelines.md
 ↓
relevant implementation
```

### AI-assisted work

Start with:

```text
AGENTS.md
 ↓
ai-workflow.md
 ↓
task-specific documentation
```

---

## Documentation Principles

### Current behavior is authoritative

Documentation must not claim that a feature exists if it does not exist in the current implementation.

### Specifications precede implementation

For meaningful behavioral changes, define the expected behavior before modifying the implementation.

### Avoid duplication

A rule should have one canonical location.

Other documents should reference that rule instead of reproducing it.

### Keep documents actionable

Documentation should help developers answer concrete questions about KNS.

Avoid generic process language that does not affect the project.

---

## Repository Relationship

This documentation does not replace:

* source code;
* tests;
* Git history;
* GitHub Issues.

Instead, it connects those artifacts into a coherent engineering process.

```text
GitHub Issues
      │
      ▼
Specification
      │
      ▼
Implementation
      │
      ▼
Tests
      │
      ▼
Decision / Delivery records
```

---

## Change Requirement

When a code change alters externally observable behavior, determine whether one or more documents in this directory must also be updated.

A completed change should leave code, tests, and documentation consistent.
