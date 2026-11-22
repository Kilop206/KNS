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
| `simulation.config.schema.json` | KNS headless batch runner | Replay and result tooling |
| `simulation.result.schema.json` | KNS headless batch runner | Result and experiment tooling |
| `experiment-analysis-v1.schema.json` | Saved-experiment analyzer | CI and report consumers |
| `operator-plan-v1.schema.json` | Network Operator CLI | Operation review and validation tooling |
| `operator-plan-comparison-v1.schema.json` | Offline plan comparator | Hashed plan review evidence with bounded differences |

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

Topology v1 validates optional node state (`active`), link state (`up`), and MAC
metadata (`mac`) with the types used by the simulator. The KNS loader and the
Discovery snapshot profile support up to 4,096 nodes.

The batch runner writes `simulation.result.json` for each successful execution.
It embeds the effective simulation config, SHA-256 of the selected executable at
batch start and of the input snapshot, plus the final counters and logical times
from KNS CSV v1. These metrics describe simulated traffic. Timing fields use
seconds. Optional queue counters are omitted when the engine CSV omits them.
Failed executions remain available in the batch summary with their status and
diagnostics.

To replay, use the executable whose hash matches `engine_sha256` and the topology
copy identified by `topology_snapshot` and `topology_sha256`. Convert the 16-digit
hexadecimal `seed` to decimal for `--seed`, set `KNS_AUTO_START` to `1` or `0`, and
apply packet size, routing metric, congestion control, and link events as recorded.
Run IDs and UTC generation times identify individual executions. The config and
metrics are the comparable parts of results from identical runs.

Saved-experiment analysis emits `analysis.json` using
[`experiment-analysis-v1.schema.json`](experiment-analysis-v1.schema.json).
Its status and exit code must agree. Completed analyses include source run and
configuration identities, input hashes and tool provenance. Error records
created after analysis starts include a diagnostic and exit code 1.
Run `python scripts/validate_contracts.py --experiment-analysis <analysis.json>`
to check an emitted result as well as the schema fixtures. Repeat the option to
check several results.

Optional `validation_rules_sha256` identifies the stored analysis rules.
`validation_rules_source_sha256` identifies externally supplied rules when
`--rules` is used. A supplied-rules identity requires a stored-rules identity
and a validation summary. Source execution status and configuration identity
continue to describe the original experiment.

Optional `evidence_integrity` records the numbers of cases with checked artifact
hashes, legacy cases without recorded hashes and verified artifact files. New
benchmark cases include `artifact_sha256` maps for their recorded evidence.
The analyzer checks these maps before creating reports and includes the coverage
in its overview.

Optional `metric_consistency` counts cases whose cached benchmark metrics were
reconciled with the CSV and cases with unavailable statistics. Reconciliation
uses the runner's aggregate CSV parser and derived-metric definitions.

Optional `auxiliary_reports` counts validated Analytics and Fault Lab reports,
with optional `traffic_reports` coverage for Traffic Lab,
and cases with unavailable execution evidence. Their content is checked against
the configuration embedded in the source profile using the native runner's
semantic validators.

Optional `comparison_baseline` and `comparison_baseline_overridden` record the
reference selected for an offline analysis. An explicit override requires the
reference name. The source configuration identity continues to identify the
saved execution inputs.

`discovery-diff-v1` describes an initial observation with
`baseline_available: false` using additions only. Its removal and change lists
must be empty or omitted. For each of nodes and links, consumers additionally
check that added, removed and changed identities are disjoint. The schema covers
the initial-observation rule; cross-list checks belong to consumer validation.

Saved experiment manifests are read within a 16 MiB byte budget. The shared
artifact reader rejects duplicate JSON fields and non-finite numbers, including
overflowing exponents, before replay or validation can interpret the input.

`operator-plan-v1` records an informational plan with target identity, action,
rendered steps and execution timeout. `mode` is always `plan`; producing or
validating it does not execute a device operation. The apply flag must agree with
the action category and every step must match the selected transport. A read-only
HTTPS action requires GET. Authentication environment values and credential-file
configuration are not part of the generated device metadata.

Validate emitted plans with `python scripts/validate_contracts.py --operator-plan plan.json`.
Compare two saved plans with `scripts/compare_operator_plans.py` and validate its
report using `--operator-comparison review/comparison.json`. See the
[comparison guide](../docs/operator-plan-comparison.md) for exit codes, limits
and the additional count invariants consumers must check.
The option is repeatable for a batch of plans.

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
Run `python scripts/validate_simulation_result.py --work-dir build` to validate
results from two parallel runs of the bundled topology fixture.
