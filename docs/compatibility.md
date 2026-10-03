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


## Coordinated release checklist

A coordinated ecosystem release does not require matching version numbers.
It requires compatible contracts and green component pipelines.

1. Merge all contract changes into KNS `main`.
2. Wait for the KNS ecosystem contract integration workflow to pass against the
   current `main` branches of Discovery, Topology Hub, Sentient KNS and KiWi.
3. Tag producer/consumer components only after their repository CI is green.
4. Publish KNS Discovery before KNS when a topology producer change is involved.
5. Publish KiWi before Sentient KNS when an intelligence/chat behavior change is
   involved, then verify the gateway against that KiWi release.
6. Publish Topology Hub before KNS when Hub API or desktop authentication changes
   are involved.
7. Publish KNS after its direct dependencies are available.
8. Record the exact component tags and KNS commit used for any benchmark or
   reproducible experiment.
9. Do not reuse or move release tags. A corrected artifact receives a new patch
   version.
10. Treat any breaking contract change as a new contract version, even when the
    product change would otherwise look like a patch release.

### Suggested first coordinated line

The current intended baseline is:

- KNS `1.1.x`;
- topology schema `1.0`;
- experiment CSV schema `1`;
- intelligence/chat contracts `v1`.

Discovery, Topology Hub, Sentient KNS and KiWi retain independent semantic
versions. Their release notes should state which KNS contract versions they
produce or consume.
