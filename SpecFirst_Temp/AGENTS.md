# KNS — SpecFirst Engineering Contract

## 1. Purpose

This document is the root engineering contract for the KNS (Kinetic Network Simulator) repository.

KNS is a C++20 discrete-event network simulator with a graphical interface based on Dear ImGui, GLFW, and OpenGL. The project models network topology, links, packet transmission, routing, TCP-like communication, simulation events, and related runtime behavior.

This document defines how the project must be understood, modified, tested, and documented.

When a more specific document conflicts with this file, the more specific document may refine implementation details, but it must not contradict the architectural and governance rules defined here.

---

## 2. Source of Truth

For repository analysis and implementation work:

1. The current state of the target Git branch is the primary source of truth.
2. For work on the TCP development line, use branch `tcp`.
3. Do not assume that a local working tree is equivalent to GitHub.
4. User-provided local code supersedes the repository only for the explicitly supplied code.
5. Issues must be evaluated against the current implementation before being implemented.

The relevant sequence is:

```text
Current repository state
        ↓
Current branch
        ↓
Issue / requirement
        ↓
Existing implementation
        ↓
Specification
        ↓
Implementation
        ↓
Tests
        ↓
Documentation
```

---

## 3. Project Principles

KNS follows these principles:

### 3.1 Deterministic simulation

Simulation behavior must be reproducible whenever the same topology, configuration, initial state, and event sequence are provided.

Logical simulation time must not be confused with wall-clock time.

### 3.2 Event-driven execution

Network behavior is modeled through scheduled events rather than arbitrary real-time delays.

Changes to event scheduling must preserve event ordering and deterministic behavior.

### 3.3 Explicit domain boundaries

Core simulation logic must remain independent from the GUI.

The project should preserve clear boundaries between:

```text
Core
├── topology
├── links
├── packets
├── routing
├── simulation
└── TCP

Application / GUI
├── rendering
├── interaction
├── configuration
└── visualization

Tests
└── unit / integration / discovery infrastructure
```

### 3.4 Fail explicitly

Invalid states and invalid API usage should be detected as close as possible to their origin.

Do not silently convert invalid operations into successful operations.

### 3.5 Documentation follows implementation

Documentation must describe the behavior that actually exists.

Do not document planned functionality as implemented functionality.

---

## 4. Architecture Rules

### 4.1 Core must not depend on GUI

Core simulation components must not depend on:

* Dear ImGui;
* GLFW;
* OpenGL;
* GUI-specific state;
* rendering abstractions.

GUI code may depend on the core.

### 4.2 Simulation state is explicit

Simulation lifecycle must be represented explicitly.

The intended lifecycle is:

```text
READY
  ↓ Start
RUNNING
  ↓ Pause
PAUSED
  ↓ Resume
RUNNING
  ↓ no events remain
FINISHED
```

Loading a topology establishes the initial simulation state according to whether executable simulation events already exist.

The absence of events before simulation execution must not automatically be interpreted as a finished simulation.

### 4.3 Network construction is not simulation execution

Creating or connecting network entities must be distinguishable from processing simulation events.

For example:

```text
Connect nodes
    ↓
Create / schedule connection
    ↓
Remain paused
    ↓
Start simulation
    ↓
Process SYN / SYN-ACK / ACK / DATA / FIN
```

User interaction that creates a TCP connection must not implicitly start the simulation engine.

---

## 5. Network Model

KNS currently models, among other concepts:

* nodes;
* links;
* packets;
* topology;
* routing;
* event scheduling;
* TCP connections and sessions.

Links have operational state and transmission characteristics.

A link that is `DOWN` must not be treated as available for routing or packet transmission.

Routing decisions must therefore reflect the current topology and link state.

---

## 6. TCP Rules

The TCP implementation is a simplified simulation model, not a full implementation of the TCP protocol.

Current behavior includes:

```text
SYN
 ↓
SYN-ACK
 ↓
ACK
 ↓
DATA / ACK
 ↓
FIN
 ↓
TIME_WAIT
 ↓
connection expiration
```

TCP changes must preserve:

* explicit connection states;
* sequence/acknowledgment semantics;
* deterministic event scheduling;
* correct handling of connection termination;
* separation between connection creation and simulation execution.

TCP behavior must be tested through observable state transitions and event outcomes.

---

## 7. API and Contract Rules

Public APIs must have clear contracts for:

* valid input;
* invalid input;
* return values;
* ownership;
* lifetime;
* bounds;
* failure behavior.

Particular attention is required for APIs involving:

* topology mutation;
* node lookup;
* link lookup;
* next-hop selection;
* packet transmission;
* event scheduling.

A function must not report success when the underlying operation failed.

---

## 8. Topology Mutation

Topology modifications are potentially observable by routing and scheduled events.

Changes to topology must define how already scheduled events behave.

When introducing dynamic topology changes, explicitly consider:

* events already in the queue;
* packets already in transit;
* links that become `DOWN`;
* nodes or links that are removed;
* routes computed before the mutation;
* stale references.

Do not assume that a topology mutation automatically invalidates or updates previously scheduled events unless the implementation explicitly guarantees it.

---

## 9. Packet and Link Identity

Packets in transit must have an unambiguous relationship with their transmission context whenever that context affects correctness.

If a packet is associated with a specific link transmission, that identity must not depend on ambiguous reconstruction from mutable topology state.

Changes involving packet transport must preserve enough information to determine the intended transmission path.

---

## 10. Testing

Every behavioral change must include appropriate tests.

At minimum:

* build the project;
* execute relevant unit tests;
* execute relevant integration tests;
* verify test discovery where applicable;
* verify headless behavior when the affected component supports it.

Tests must validate behavior rather than implementation details whenever possible.

A test that merely exercises code without asserting meaningful behavior is insufficient.

---

## 11. GUI Rules

GUI code is responsible for presentation and user interaction.

GUI controls must not introduce hidden simulation semantics.

For simulation controls:

```text
Start
Pause
Resume
Step
```

must correspond to explicit simulation-engine behavior.

Configuration controls must not silently execute simulation unless explicitly defined as execution controls.

---

## 12. Build and Toolchain

KNS is a C++20 project.

The intended Windows MinGW toolchain is:

```text
C:\mingw64
```

Do not mix incompatible compiler/runtime environments.

In particular, do not accidentally combine:

```text
C:\mingw64
```

with binaries or runtime libraries originating from:

```text
C:\msys64\ucrt64
```

Build configuration must use a consistent compiler, linker, runtime, and dependency environment.

---

## 13. Issue Workflow

An open issue is not automatically a valid implementation task.

Before implementing an issue:

1. Read the issue.
2. Inspect the current branch.
3. Locate the affected implementation.
4. Determine whether the issue is:

   * resolved;
   * partially resolved;
   * still valid;
   * obsolete.
5. Define the remaining behavioral requirement.
6. Update the relevant specification if necessary.
7. Implement the smallest coherent change.
8. Add or update tests.
9. Run validation.
10. Document the resulting decision.

---

## 14. SpecFirst Workflow

For non-trivial changes:

```text
Requirement
    ↓
Specification
    ↓
Acceptance criteria
    ↓
Implementation plan
    ↓
Implementation
    ↓
Tests
    ↓
Review
    ↓
Documentation
```

Specifications must describe observable behavior.

Implementation must not be used as a substitute for defining requirements.

---

## 15. Documentation Rules

The following documents form the project documentation system:

```text
docs/
├── project-overview.md
├── architecture.md
├── domains.md
├── data-model.md
├── workflows.md
├── ai-workflow.md
├── coding-standards.md
├── testing.md
├── security.md
├── design-guidelines.md
├── operations.md
├── tooling-adapters.md
├── implementation-governance.md
├── implementation-plan.md
├── issues.md
├── decision-log.md
└── deployment-log.md
```

Documentation must not duplicate information unnecessarily.

If the same rule belongs to multiple documents, define it once in the most appropriate document and reference it elsewhere.

---

## 16. AI-Assisted Development

AI-assisted development must follow the same engineering rules as human development.

AI must:

* inspect the current repository state;
* avoid assuming outdated code;
* identify uncertainty;
* avoid inventing APIs;
* avoid claiming tests were executed when they were not;
* avoid treating generated code as automatically correct;
* preserve project architecture;
* update documentation when behavior changes.

When repository state and conversation history disagree, the current repository takes precedence unless the user explicitly provides local code as the source for the task.

---

## 17. Security

Never commit:

* access tokens;
* API keys;
* passwords;
* private credentials;
* authentication cookies;
* secrets embedded in configuration files.

Credentials must be revoked immediately if accidentally exposed.

---

## 18. Change Discipline

Prefer:

```text
small
explicit
testable
reviewable
reversible
```

changes.

Avoid unrelated refactoring during issue implementation.

Do not modify architecture merely to make a local implementation easier.

---

## 19. Completion Criteria

A change is complete only when:

* the intended behavior is implemented;
* relevant tests pass;
* invalid behavior is handled appropriately;
* documentation is synchronized;
* no known regression is introduced;
* the resulting diff is understood.

The final review must compare:

```text
Requirement
    ↕
Specification
    ↕
Implementation
    ↕
Tests
    ↕
Documentation
```

All five should describe the same behavior.
