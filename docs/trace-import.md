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
events; CSV/JSON conversion and PCAP/PCAPNG parsing remain separate work. It does
not infer physical topology, reproduce protocol state or measure a real network.
