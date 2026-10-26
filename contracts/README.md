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
| `kiwi.analysis.schema.json` | KiWi `/api/v1/analyze` | Sentient KNS |
| `analysis.context.schema.json` | KNS `AIContextBuilder` | Sentient KNS, KiWi |
| `discovery.snapshot.schema.json` | KNS Discovery | Topology Hub, KNS |

Sentient KNS remains the public product gateway. KiWi's internal analysis adapter
may use a different field naming convention, but that adapter is not a public
desktop contract. `kiwi.analysis.schema.json` describes its current response
body; `/api/v1/analyze` supplies the version because the body has no version field.
It does not claim measurement authority or add request provenance absent from the
existing response. Correlation and grounding evidence belong in the request and
gateway envelope until a versioned response migration is agreed.
`analysis.context.schema.json` describes KNS deterministic topology analysis.
Its critical-node/link lists are bounded summaries, so missing entries must not
be treated as proof that an entity is safe. Packet-level simulation counters are
carried separately by the intelligence request when available.
`discovery.snapshot.schema.json` refines topology v1 with the evidence-bearing
fields emitted by the current Go collector. It does not invent timestamps for
observations the collector has not recorded. The `observed_at` UTC field is an
optional extension until a versioned producer migration supplies it.
`discovery.snapshot.valid.traffic.json` is a synthetic three-node fixture for
offline integration tests. Its nodes and inferred links are test data, not
observations of a real network.

## Compatibility policy

- additive optional fields are allowed within a major contract version;
- removing or renaming fields, changing requiredness, or changing semantics
  requires a new major schema file;
- producers must emit a declared schema version when that contract includes one;
- consumers must reject unsupported major versions instead of guessing;
- contract fixtures used in one repository must remain valid against this source.

Install `python -m pip install -r scripts/requirements-contracts.txt`, then run
`python scripts/validate_contracts.py` to validate every JSON Schema and its
positive and negative fixtures. Fixtures live in `contracts/fixtures/`; add both
forms whenever a schema is added. Multiple negative fixtures may cover version,
requiredness, units, and boundary errors. JSON Schema cannot enforce every graph-wide
invariant (for example, whether a link endpoint names an existing node), so
consumers must retain their semantic topology validation.
The validator also checks every bundled topology in `app/topologies/` against
the canonical topology v1 schema. The KNS loader still accepts legacy local
topologies without `schema_version` for backward compatibility.
After building the application, run `python scripts/validate_analysis_context.py`
to check a context emitted by the actual headless engine. The script runs a
bundled fixture topology and writes only into a temporary directory. Use
`--work-dir <directory>` to retain the generated evidence locally.
