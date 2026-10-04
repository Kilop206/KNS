# KNS ecosystem contracts

This directory is the canonical source for wire contracts shared by KNS ecosystem
components. Contracts are versioned independently from application releases.

## Ownership

| Contract | Producer | Consumer |
| --- | --- | --- |
| `topology-v1.schema.json` | KNS Discovery, Topology Hub, KNS | KNS, Topology Hub |
| `discovery-diff-v1.schema.json` | KNS Discovery | KNS, Topology Hub |
| `desktop-intelligence-request-v1.schema.json` | KNS desktop | Sentient KNS |
| `chat-request-v1.schema.json` | KNS desktop | Sentient KNS, KiWi through the gateway |
| `chat-response-v1.schema.json` | KiWi | Sentient KNS, KNS desktop |

Sentient KNS remains the public product gateway. KiWi's internal analysis adapter
may use a different field naming convention, but that adapter is not a public
desktop contract.

## Compatibility policy

- additive optional fields are allowed within a major contract version;
- removing or renaming fields, changing requiredness, or changing semantics
  requires a new major schema file;
- producers must emit a declared schema version when that contract includes one;
- consumers must reject unsupported major versions instead of guessing;
- contract fixtures used in one repository must remain valid against this source.

Run `python scripts/validate_contracts.py` to parse and sanity-check every JSON
Schema in this directory.
