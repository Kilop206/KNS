# KNS — Design Guidelines

## 1. Purpose

These guidelines define the architectural and design principles for KNS.

---

## 2. Separation of Concerns

Keep major responsibilities separated:

```text
GUI
 ↓
Application control
 ↓
Simulation
 ↓
Network / Protocol domains
```

A component should not accumulate unrelated responsibilities merely because it has access to the required data.

---

## 3. Core Independence

Core simulation behavior should not depend on GUI rendering.

The simulation must remain usable in headless execution.

---

## 4. Determinism

Simulation behavior should be deterministic for equivalent initial conditions.

Logical time and event ordering should be explicit.

---

## 5. Explicit State

Important lifecycle states should be represented explicitly.

Examples:

```text
Simulation:
Ready / Running / Paused / Finished

Link:
UP / DOWN

TCP:
protocol-specific connection states
```

Avoid inferring important state solely from incidental conditions such as an empty queue.

---

## 6. Single Source of Truth

Each important concept should have a clear authoritative representation.

Examples:

* topology connectivity → `Topology`;
* link availability → `Link`;
* simulation lifecycle → simulation state;
* TCP protocol phase → TCP state.

---

## 7. Failure Propagation

Failures must remain observable across abstraction boundaries.

For example:

```text
sendPacket()
    ↓ failure
sendPacketThroughTopology()
    ↓
must not report success
```

Higher-level functions must not conceal meaningful lower-level failures.

---

## 8. Mutable Topology

Dynamic topology changes require explicit handling.

Queued events and in-flight packets must not silently become associated with invalid or unintended entities.

---

## 9. API Contracts

Public or widely used APIs should define:

* valid input;
* invalid input;
* return semantics;
* ownership;
* side effects;
* failure behavior.

Ambiguous APIs create defects across multiple domains.

---

## 10. Minimal Coupling

Prefer narrow interfaces between components.

Avoid unnecessary dependencies between:

* GUI and network internals;
* TCP and rendering;
* topology and presentation;
* testing infrastructure and production behavior.

---

## 11. Extensibility

New protocols, packet types, or simulation features should be introduced without unnecessarily rewriting unrelated domains.

Prefer explicit extension points over hidden special cases.

---

## 12. Performance

Performance optimizations should be based on measured bottlenecks.

Do not sacrifice correctness or determinism for speculative optimization.

---

## 13. Readability

Code should communicate the simulation model clearly.

Prefer explicit logic over clever abstractions when the latter obscure network or protocol behavior.

---

## 14. Testability

Design components so that meaningful behavior can be exercised without requiring the complete GUI application.

---

## 15. Documentation Alignment

Architectural documentation must remain aligned with the implementation.

When a design decision changes, update the relevant documentation and decision log.
