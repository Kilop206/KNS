# Ordered abstract trace replay

The first Trace Import foundation extends Traffic Lab with a global ordered
source timeline. It uses real routing, serialized link transmission, propagation,
queues, loss and outcome accounting. Existing generated schedules remain compatible.

Replay plans use the existing schema version 1.0, optional root `replay_events`
and flow `schedule: {"kind": "replay"}`. An event is `{at_s, flow_id}`. Flows
still declare modeled endpoints, fixed wire size and start/end window. Events
must be time-ordered, may tie and must lie within their flow window. Every flow
must be referenced. Limits are 10,000 source events and 256 flow configurations.
Replay-only plans do not synthesize application responses or mix generated
schedules into the captured timeline.

```powershell
.\build-core-mingw\verification-build\app\KNS.exe --headless --topology "KNS Experiments/traffic-lab/topology.json" --traffic-plan contracts/fixtures/traffic-plan-v1.valid.replay.json --output "results/replay.csv"
```

There is one queued replay-source event at a time; each dispatch emits its row
and schedules the next row. Array order is therefore preserved for simultaneous
sources, including multiple rows in one flow. Grouping into independently
scheduled custom flows would reorder some ties and change queue/drop outcomes.
Ordinary network arrivals still follow the scheduler's timestamp/event-ID rule.
Tied source order does not imply an imposed order for all network events.

The normal `--traffic-plan`, experiment manifest and result-validation paths apply.
Time/event interruption retains the unexecuted source suffix; resuming the same
engine continues it. An insufficient packet budget is explicitly incomplete,
even when all already-generated packets have drained. No replay event consumes
a traffic RNG draw. Outcomes remain generated = delivered + dropped + pending.

Native tests check tied source order, queue rejection identity, bounded pending
source work, resume, budget truncation and atomic invalid-plan rejection. Python
preflight checks match runtime constraints and actual CLI integration covers the
complete and budget-truncated paths. This foundation accepts explicit modeled
events. CSV/JSON conversion is described below; PCAP/PCAPNG remain pending. It does
not infer physical topology, reproduce protocol state or measure a real network.

## Import CSV or JSON packet metadata

```powershell
python scripts/trace_import.py "KNS Experiments/trace-lab/packets.csv" --topology "KNS Experiments/trace-lab/topology.json" --output results/imported-trace --exe build-core-mingw/verification-build/app/KNS.exe
```

The bundled CSV and equivalent JSON are authored analytical inputs, not captured
real-network measurements. Their burst exercises a two-slot queue: five offered
packets produce four deliveries and one actual simulated drop. Remove `--exe` to
generate inputs only. Output directories must be new. Optional execution uses the
profile runner and retains a portable manifest; replay needs no original trace.

CSV requires exactly `time_s,flow_id,source,destination,wire_bytes`. JSON uses
schema version 1.0 with a `packets` array of those fields; see
`contracts/traffic-trace-v1.schema.json`. Times are numeric seconds; labels are
short strings/integers. Endpoints are explicitly mapped model-node indices in the
supplied topology. Wire bytes are provided by the user for the desired accounting;
headers/protocol state are not inferred and payload fields are rejected. Ordered
timestamps may tie; source row order is preserved. Absolute time origin is removed
before conversion to binary64 virtual seconds. Decimal parsing preserves small
epoch differences; remaining clock-resolution collisions are explicitly counted.

Grouping uses logical source flow, endpoints and fixed wire size. A variable-size
captured flow therefore produces multiple native Analytics groups. Opaque
`source_flow_id` entries in `trace-metadata.json` retain correlation across those
groups; do not combine separate percentiles as if they were an original-flow
percentile. Metadata includes source/importer/plan hashes, packet/wire totals,
size distribution and interarrival histogram/moments over normalized float offsets.

Raw traces are never copied into bundles or sent externally. Original labels are
omitted by default; `--keep-flow-labels` retains them explicitly. The epoch origin
is not exported. Topology labels/indices and timing can still identify a network;
this is selected identifier suppression, not comprehensive anonymization. Inspect
files before sharing them.

Limits: 4 MiB per trace/topology, 10000 packets, 256 endpoint/flow/size groups,
1 MiB per modeled packet, normalized duration below 1000000 seconds. Duplicate
JSON fields, malformed/extra/missing columns, unsupported fields, unknown nodes,
empty/unsorted/non-finite inputs and excess limits fail before output creation.
Excess data is rejected rather than truncated. Topology preflight is structural;
the native loader validates it on execution. PCAP/PCAPNG and protocol statistics
are pending. No captures, elevated privileges or real-network changes are needed.
