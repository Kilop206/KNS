# KNS ecosystem release contract

This document defines the compatibility gates for coordinated KNS ecosystem
releases. Application versions may advance independently, but a supported bundle
must agree on the contracts and API boundaries below.

## Components and responsibilities

| Component | Responsibility | Compatibility boundary |
| --- | --- | --- |
| KNS | deterministic simulation, topology editing and desktop UX | topology v1, desktop intelligence v1, chat v1 |
| KNS Discovery | observes a local network and emits snapshots | topology v1 |
| Topology Hub | stores, versions and collaborates on topology documents | topology v1 + `/api/topologies` |
| Sentient KNS | product gateway for intelligence | desktop intelligence v1 + chat v1 |
| KiWi | interprets deterministic KNS facts | gateway adapter + chat v1 |

The canonical JSON Schemas are under `contracts/` in this repository.

## Current contract generation

- topology: `1.0`;
- desktop intelligence request: `v1`;
- chat request: `v1`;
- chat response: `v1`;
- headless aggregate CSV: schema `1`.

An additive optional field can remain within the same major contract. Removing
or renaming a field, changing its requiredness or changing semantics requires a
new major contract.

## Release gates

A coordinated release is ready only when:

1. KNS passes Windows, Linux and macOS build/test jobs, Linux sanitizers, warnings-as-errors and contract validation.
2. KNS Discovery passes `go test ./...`, `go vet ./...` and builds on Windows and Linux.
3. Topology Hub passes Quarkus tests, frontend type/build checks and PostgreSQL + Playwright E2E.
4. Sentient KNS passes Maven verification on Windows and Linux.
5. KiWi passes its offline regression matrix and deterministic benchmark.
6. Consumer fixtures validate against the canonical contract revision.
7. A Hub-backed topology can be loaded, edited and saved from KNS without bypassing optimistic locking; the save creates a Hub revision.
8. The exact KNS commit, topology revision, random seed and contract generation are retained with reproducible experiment results.

## Compatibility metadata

Release notes should include KNS, Discovery, Topology Hub, Sentient KNS and KiWi commit/tag identifiers, the contract generations, the KiWi base model and any adapter/training manifest.

Do not describe two component versions as compatible merely because their processes start successfully. The boundary must be exercised by the contract or integration tests above.

## Branch policy after TCP promotion

`main` is the canonical integration and release line. New work uses short-lived feature branches and enters `main` only through green CI. The historical `tcp` line is no longer a feature-specific release line after its promotion. Older divergent branches such as `v1.0` remain historical references rather than merge targets.
