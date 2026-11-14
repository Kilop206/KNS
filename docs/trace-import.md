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
events. CSV/JSON conversion and a PCAP subset are described below. It does
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
the native loader validates it on execution. No captures, elevated privileges or
real-network changes are needed.

## Classic PCAP metadata subset

```powershell
python scripts/trace_import.py capture.pcap --topology model.json --address-map ipv4-map.json --output results/pcap --exe build-core-mingw/verification-build/app/KNS.exe
```

The explicit mapping uses schema version 1.0 and `ipv4_to_node`, for example
`{"schema_version":"1.0","ipv4_to_node":{"192.0.2.10":0,"192.0.2.20":1}}`.
These are documentation addresses; supply mappings appropriate to your own model.
Version 1.0 remains unchanged. Version 1.1 adds optional `ipv6_to_node`; at least
one family must be present and each present family must be nonempty. See
`contracts/pcap-address-map-v1.1.schema.json` and the bundled
`address-map-dual-stack.json`. Runtime validation requires canonical unscoped
address strings and at most 4096 combined entries.
Every observed endpoint must map to a valid model node. The map is not copied;
its canonical hash is recorded. `--keep-flow-labels` would retain the original
address/port/protocol tuples and should be chosen deliberately before sharing.

The reader supports PCAP 2.4 with either byte order, microsecond/nanosecond
timestamps and plain Ethernet link type 1. It extracts unfragmented IPv4 TCP/UDP
and IPv6 direct TCP/UDP endpoint/port metadata, untagged or behind at most two VLAN
tags (TPIDs 0x8100 and 0x88a8). Header/length bounds and UDP length/TCP data
offsets are checked. Fully captured necessary headers allow record truncation
afterward; `truncated_records` reports captured length below original length.
Modeled bytes use original PCAP record length, including its recorded link header,
without adding preamble, interframe gap or FCS. This is a declared byte scope,
not automatic reconstruction of physical wire overhead. Checksums are not checked
and payload bytes are not exported. Protocol counts are observed TCP/UDP records.

VLAN tag bytes remain part of original record length. Ordered TPID/VID stacks
separate logical source flows, including tagged versus untagged records. PCP/DEI
bits do not split flows and do not create modeled priority or drop eligibility.
VLAN switching, isolation, trunk configuration and forwarding rules are not
inferred. Metadata reports tagged record count and maximum depth; identifiers
remain suppressed unless `--keep-flow-labels` is selected. Unknown TPIDs, more
than two tags and incomplete tag/inner-type headers fail the entire import.

IPv6 import supports the fixed 40-byte base header followed directly by TCP/UDP.
Payload length must be nonzero and fit the original record. Extension headers,
fragments (including atomic fragments), jumbograms, ICMPv6 and other next headers
are rejected. IPv6 flow labels, traffic class and hop limit do not create modeled
QoS, forwarding or multicast semantics; each record is offered between its two
explicitly mapped model nodes. `ip_version_counts` reports record counts by IP
version. This subset does not claim general IPv6 capture support.

References: [libpcap savefile specification](https://github.com/the-tcpdump-group/libpcap/blob/master/pcap-savefile.manfile.in),
[IPv4](https://www.rfc-editor.org/rfc/rfc791.html),
[IPv6 base header](https://www.rfc-editor.org/rfc/rfc8200.html#section-3),
[UDP](https://www.rfc-editor.org/rfc/rfc768.html),
[TCP header format](https://www.rfc-editor.org/rfc/rfc9293.html#section-3.1).
Tag layout and supported TPIDs follow the
[Wireshark VLAN description](https://wiki.wireshark.org/VLAN/) and
[Wireshark EtherType definitions](https://www.wireshark.org/docs/wsar_html/etypes_8h.html).
The implementation is a bounded, read-only standard-library subset rather than a
general libpcap dissector. It keeps native builds independent of capture libraries.
PCAPNG, IPv6 extensions, other link types, additional PCAP link flags, fragmented traffic,
other IP protocols and incomplete necessary headers are explicitly rejected.
Records are never silently skipped; broader support needs a separate adapter.

Each accepted record becomes an abstract offered wire packet, including captured
control packets/retransmissions. There is no TCP state, application replay or
deduplication. Observation times schedule source events, and subsequent latencies
come from the model; they do not measure original end-to-end delays. Captures do
not reveal uncaptured traffic or physical topology automatically.

Tests construct clearly authored binary fixtures for four byte-order/resolution
combinations, complete/truncated records, malformed lengths/headers, unsupported
formats, missing maps and privacy defaults. Actual native replay exercises queue
contention, original-length byte accounting and portable manifest reconstruction.
Those fixtures are not real-network measurements, and no live capture is performed.
