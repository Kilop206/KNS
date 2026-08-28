# KNS — Workflows

This document defines the principal workflows used by KNS during simulation execution and development.

---

## 1. Simulation Workflow

The standard simulation lifecycle is:

```text
Load / Configure
       ↓
READY
       ↓
Start
       ↓
RUNNING
       ↓
Pause
       ↓
PAUSED
       ↓
Resume
       ↓
RUNNING
       ↓
Event queue exhausted
       ↓
FINISHED
```

---

## 2. Loading a Topology

The topology-loading workflow is:

```text
Topology file
      ↓
TopologyLoader
      ↓
Validation
      ↓
Topology construction
      ↓
Simulation initialization
```

Invalid input must be rejected before invalid topology state is used by the simulation.

After loading, the simulation lifecycle state must reflect whether execution has actually begun and whether executable events are available.

---

## 3. Creating a Network Connection

Creating a connection is separate from running the simulation.

Example GUI workflow:

```text
User interaction
      ↓
Select / connect nodes
      ↓
Create or schedule TCP connection
      ↓
Events enter simulation queue
      ↓
Simulation remains paused
```

The operation must not implicitly process the resulting events.

---

## 4. Starting Simulation

When the user explicitly starts the simulation:

```text
READY
  ↓
Start
  ↓
RUNNING
  ↓
Process queued events
```

The engine begins processing events according to its scheduling rules.

---

## 5. Pausing Simulation

When running:

```text
RUNNING
    ↓
Pause
    ↓
PAUSED
```

The pause operation stops further event processing while preserving the simulation state and pending work.

---

## 6. Resuming Simulation

When paused:

```text
PAUSED
    ↓
Resume
    ↓
RUNNING
```

Pending events remain available for processing.

---

## 7. Stepping Simulation

When supported:

```text
PAUSED
    ↓
Step
    ↓
Process one logical unit of scheduled work
    ↓
PAUSED
```

Step execution must not implicitly transition the simulator into uncontrolled continuous execution.

---

## 8. Simulation Completion

Completion occurs when:

```text
Simulation is executing
        ↓
No executable events remain
        ↓
FINISHED
```

The important distinction is:

```text
Initial empty queue
        ≠
Completed execution
```

An empty queue alone must not incorrectly mark a never-started simulation as finished.

---

## 9. TCP Connection Workflow

A simplified TCP workflow is:

```text
Create connection
      ↓
SYN
      ↓
SYN-ACK
      ↓
ACK
      ↓
Established
      ↓
DATA
      ↓
ACK
      ↓
FIN
      ↓
TIME_WAIT
      ↓
Expiration
```

The exact state transitions are determined by the implementation.

---

## 10. Packet Transmission Workflow

Conceptually:

```text
Packet generated
      ↓
Determine route
      ↓
Select next hop / link
      ↓
Check link state
      ↓
Check transmission constraints
      ↓
Queue / schedule transmission
      ↓
Advance through simulation
      ↓
Deliver packet
```

A `DOWN` link must prevent normal transmission through that link.

---

## 11. Routing Workflow

A routing request follows:

```text
Source
  ↓
Destination
  ↓
Topology inspection
  ↓
Available links
  ↓
Path calculation
  ↓
Next hop
```

Routing must reflect the current operational state of links.

---

## 12. Dynamic Topology Workflow

When topology changes while events are pending:

```text
Existing event queue
        ↓
Topology mutation
        ↓
Re-evaluate affected behavior
        ↓
Process future events according to defined contracts
```

The implementation must explicitly determine how already scheduled events interact with the new topology.

Do not assume that all queued events automatically become invalid or automatically adapt.

---

## 13. Link Failure Workflow

When a link changes from `UP` to `DOWN`:

```text
Link UP
  ↓
Link DOWN
  ↓
Routing excludes link
  ↓
New transmissions cannot use link
```

The behavior of packets already in transit must follow the packet/event contract.

---

## 14. Issue Resolution Workflow

An issue is resolved through:

```text
Issue
 ↓
Inspect current branch
 ↓
Inspect current implementation
 ↓
Classify issue
 ↓
Define remaining requirement
 ↓
Update specification
 ↓
Implement
 ↓
Test
 ↓
Review
 ↓
Document
```

Possible classifications:

```text
Resolved
Partially resolved
Still valid
Obsolete
```

---

## 15. Specification-First Workflow

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
Code
     ↓
Tests
     ↓
Review
     ↓
Documentation
```

The specification must describe observable behavior rather than implementation guesses.

---

## 16. AI-Assisted Workflow

AI-assisted development follows:

```text
Task
 ↓
Read repository state
 ↓
Read applicable instructions
 ↓
Read relevant specification
 ↓
Inspect implementation
 ↓
Propose change
 ↓
Implement
 ↓
Run tests
 ↓
Inspect diff
 ↓
Update documentation
```

The AI must not assume that previous conversation state represents the current repository.

---

## 17. Build and Validation Workflow

A normal validation sequence is:

```text
Clean / configure build
        ↓
Compile
        ↓
Run tests
        ↓
Run relevant headless scenarios
        ↓
Inspect failures
        ↓
Review diff
```

The exact commands depend on the configured toolchain and build system.

---

## 18. Documentation Workflow

When externally observable behavior changes:

```text
Implementation change
       ↓
Identify affected specification
       ↓
Update documentation
       ↓
Review consistency
       ↓
Commit together
```

Documentation should not describe future behavior as if it were already implemented.
