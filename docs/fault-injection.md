# Fault Lab: scheduled link experiments

The link Fault Lab executes real discrete-event model mutations in `KNS::kns`.
It supports link down/up, bandwidth changes (Mbps), propagation delay changes
(milliseconds) and loss probability changes ([0,1]). Repeated down/up events
model flapping. Temporary degradation requires an explicit restoration event
with the desired value; no implicit timer resets a parameter.

## Run a controlled resilience experiment

The bundled profile keeps the same topology/workload and two seeds across a
control, a link outage and a link degradation. All variants observe the same
timestamps so their observation horizon is controlled. Compare aggregate
outcomes, then inspect each case's report for before/during/after counters and
whether the route was disconnected. Several degraded parameters change together;
that treatment does not isolate the effect of any single parameter.

```powershell
python scripts/experiment_profile.py --profile "KNS Experiments/fault-lab.json" --exe build-core-mingw/app/KNS.exe --output results/fault-lab
python scripts/experiment_profile.py --replay results/fault-lab/manifest.json --exe build-core-mingw/app/KNS.exe --output "results/fault replay"
```

Each variant may embed `fault_plan` in its experiment definition. The profile
runner validates it before creating artifacts, writes a per-topology plan, runs
the CLI, verifies the applied-event/observation evidence and preserves report
paths in `runs/benchmark.json`. Plans are included in the manifest configuration
hash and reconstruct on replay. Comparisons retain the existing paired seed/
payload/routing/TCP settings. This profile uses the established automatic TCP
workload; it does not measure HTTP/DNS service recovery.

## Direct CLI

Save this plan as `fault.json`:

```json
{
  "schema_version": "1.0",
  "in_flight_policy": "complete_accepted_hops",
  "probes": [{"source": 0, "destination": 2}],
  "observe_at": [0, 0.01, 0.025, 0.04, 0.06, 0.08, 0.1, 0.2, 0.5, 1, 2],
  "events": [
    {"at": 0.025, "type": "link_down", "link_index": 0},
    {"at": 0.08, "type": "link_up", "link_index": 0}
  ]
}
```

```powershell
.\build-core-mingw\app\KNS.exe --headless --topology app/topologies/services-lab.json --fault-plan fault.json --seed 42 --output "results/outage run.csv"
```

The CLI writes the aggregate CSV and `results/outage run.csv.faults.json`. A time/
event limit still returns code 3 and preserves partial results with pending work;
such a report is not complete. Invalid plans return 1 before simulation/export.
Legacy `--link-event` remains available. Prefer one fault mechanism per run to
keep the treatment attributable; legacy events are not included in the new
plan's applied-event log.

`link_index` selects the zero-based link array entry in the loaded topology.
Scheduling resolves it to the exact stable runtime ID, so parallel links are
unambiguous. Reports use original array indices rather than process-global IDs
so equivalent replay reports remain comparable. Removing a scheduled target
before dispatch fails execution explicitly. Missing/invalid targets are not
silently treated as applied faults.

## Time and packet policy

Event timestamps and observations are virtual seconds. All faults at a timestamp
execute in file order; observations created by that plan at the same timestamp
see the final planned state. Other events follow the existing timestamp/event-ID
order. Observations are callbacks, not an assertion that every packet arrival at
that exact floating-point timestamp has already executed.

An observation at the scheduling clock (zero in the CLI) and at every fault time
is added automatically. `observe_at` adds other samples; duplicates coalesce.
Observation events can extend the simulation duration after traffic has drained.
Use equal horizons when comparing throughput; wall time never substitutes for
virtual time. All APIs operate on the simulation owner thread.

The only accepted in-flight policy is `complete_accepted_hops`. Packets already
accepted by a link, including future queued reservations, retain their scheduled
serialization/propagation timing through down/up and parameter changes. New sends
observe availability, routes and current parameters. Intermediate forwarding
consults the new route; no-route drops use existing accounting. Queues currently
hold entries until arrival, including propagation. Global loss overrides, if
explicitly enabled through the Core API, take precedence over link loss settings.

## Interpret the report

- `changes`: applied events with previous values and virtual timestamps.
- `observations`: cumulative raw counters, pending packets, link availability/
  parameters/occupancy and route traces for each configured source/destination.
  These route probes inspect tables without generating traffic.
- `windows`: differences between consecutive observation counters, delivered
  packets per virtual second and delivered DATA mean latency for that interval.
  Latency is grouped by delivery observation, even for packets created earlier.
  No delivered DATA gives null mean latency, not zero. Counters retain their
  [existing metric definitions](experiments.md#current-measurements).
- `sampled_link_outages`: observed down intervals for each original link.
- `sampled_connectivity_outages`: observed unreachable/invalid/loop intervals per
  probe. A failed link with a working alternate route produces no connectivity
  outage. Open intervals are censored with null end/duration; observation ends
  do not imply recovery.
- `routing_model`: synchronous recomputation, not a distributed convergence
  protocol. `service_recovery_s` is null because this report does not measure
  successful application operations after restoration.

Intervals are sampled evidence. All planned link mutations are sampled at their
exact timestamps, but changes made by other mechanisms between observations may
be missed. Initial disconnection is left-censored by the first observation even
when the report's start is numeric; it does not establish when failure began.
No causal attribution, unique-payload goodput, per-flow loss/deadline recovery or
automatic single-point-of-failure conclusion is inferred from these counters.

## Validation and boundaries

[Fault plan schema](../contracts/fault-plan-v1.schema.json) and profile schema
fixtures are validated by the existing contract CI. Runtime validates target
indices, active/distinct probe endpoints, finite domains and times before
scheduling anything. Limits: CLI input 4 MiB; at most 10000 fault events, 10000
explicit observations, 64 probes and one million retained link samples.

Core tests cover outage/recovery, alternate routes and parallel link selection,
accepted/queued timing across failure and parameter changes, parameter restoration,
invalid-plan atomic rejection, simultaneous events, censored outages, flapping
and missing runtime targets. Python integration exercises the actual CLI,
invalid-plan rejection, paired profile execution and byte-identical report replay.

This increment does not implement node failure/recovery, device overload, induced
application traffic, dedicated gateway models, conditional/random fault triggers,
per-flow impact or measured service recovery. Disconnecting a gateway's incident
links can model connectivity interruption but is not equivalent to device failure.
Studio fault scheduling UI remains pending. No real network settings are changed.
