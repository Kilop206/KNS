# KNS contracts

This directory defines the file formats and optional client interfaces exposed by
KNS and KNS Discovery. Schemas are versioned independently from applications.

## Ownership

| Contract | Producer | Consumer |
| --- | --- | --- |
| `topology-v1.schema.json` | KNS Discovery, KNS | KNS |
| `discovery-diff-v1.schema.json` | KNS Discovery | KNS |
| `desktop-intelligence-request-v1.schema.json` | KNS desktop | Compatible analysis endpoint |
| `chat-request-v1.schema.json` | KNS desktop | Compatible chat endpoint |
| `chat-response-v1.schema.json` | Compatible chat endpoint | KNS desktop |
| `analysis.context.schema.json` | KNS `AIContextBuilder` | Analysis tooling |
| `discovery.snapshot.schema.json` | KNS Discovery | KNS |


`analysis.context.schema.json` describes KNS deterministic topology analysis.
Its critical-node/link lists are bounded summaries, so missing entries must not
be treated as proof that an entity is safe. Packet-level simulation counters are
carried separately by the intelligence request when available.
`discovery.snapshot.schema.json` refines topology v1 with the evidence-bearing
fields emitted by the Go collector. Since Discovery `9f693a3`, successful OS
collections include `observed_at` in UTC. The field remains optional for older
producers and synthetic fixtures without a collection time. An unchanged graph
retains its last published observation time so watch mode does not reload KNS.
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
