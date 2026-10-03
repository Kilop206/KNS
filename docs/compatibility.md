# KNS ecosystem compatibility

This document defines the compatibility boundary for the first coordinated KNS
ecosystem release. Product versions may advance independently; wire and file
contracts are the stable boundary.

| Producer | Consumer | Contract |
| --- | --- | --- |
| KNS Discovery | KNS, Topology Hub | topology schema `1.0` |
| Topology Hub | KNS | topology schema `1.0` plus Hub optimistic `version` |
| KNS desktop | Sentient KNS | desktop intelligence request schema v1 |
| KNS desktop | Sentient KNS / KiWi | chat request schema v1 |
| KiWi | Sentient KNS / KNS desktop | chat response schema v1 |
| KNS | experiment tooling | CSV schema `1` |

## Release rules

- A breaking topology change requires a new topology schema version.
- A breaking intelligence or chat payload change requires a new contract file.
- Producers must continue emitting the documented version until all supported
  consumers accept the replacement.
- Release notes must call out contract changes explicitly.
- KNS, Discovery, Hub, Sentient and KiWi tags are independent; matching tag
  numbers are not required.
- A release candidate is considered ecosystem-compatible only after each
  repository's own CI passes and the KNS contract-validation job passes.

## Current KNS line

KNS `1.1.x` is the first line intended to consume the coordinated contracts in
`contracts/`. The exact implementation commit and dependency versions should
be recorded in experiment metadata when reproducibility matters.
