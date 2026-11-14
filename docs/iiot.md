# Industrial telemetry and control scenarios

The first Industrial increment composes existing IoT endpoints, router forwarding,
Traffic Lab schedules, deadlines and Fault Lab events. It models explicit sensor
messages to a telemetry destination plus a PLC/actuator request-response loop.
There is no physical process, PLC program, MQTT broker/protocol, retry guarantee,
MQTT QoS or finite-capacity device processor in this abstraction.

```powershell
python scripts/industrial_scenario.py --sensors 100 --exe build-core-mingw/verification-build/app/KNS.exe --output results/industrial-100
python scripts/industrial_scenario.py --sensors 100 --redundant --exe build-core-mingw/verification-build/app/KNS.exe --output results/industrial-redundant
```

Use your own build's executable path when different. Omit `--exe` to generate
inputs only. Output directories must be new. Each bundle contains the actual
topology, traffic plan, experiment profile and `model.json`. Executed bundles add
the normal portable manifest, raw traffic/Analytics/Fault Lab evidence,
comparisons and measured `measurements.json`.

The three variants share offered sources: a 2 Mbps reference uplink, that uplink
down from virtual 0.3 through 0.5 seconds, and a 0.5 Mbps congested uplink. There
are 125-byte sensor messages every 100 ms, phase-staggered over ten 10 ms slots,
with 30 ms per-message deadlines. The PLC emits five 64-byte commands and receives
64-byte replies after a fixed 1 ms actuator processing timer, with a 20 ms operation
deadline. Priority classes are metadata under the existing FIFO/RED policy.

`--redundant` adds an alternate uplink router/path with higher propagation cost.
Fault observations show the primary two-hop sensor route switch to three hops
during the outage and back afterward. The common gateway remains a single point
of failure. Routing convergence is synchronous in the current model; connectivity
and timely message delivery are separate observations. Accepted hops complete
through failure, as declared by Fault Lab's in-flight policy.

## Initial measured scale

The generator accepts 1..250 sensors, leaving headroom within the Core's current
256-flow limit. The scenarios of 1,000 and 10,000 sensors remain pending and are
rejected explicitly. They are targets, not measured performance claims. Increasing
the flow limit alone would not validate them: the current engine materializes a
routing row for every node, with quadratic table storage, and context/export costs
also grow. Further profiling and bounded retention work are needed.

The following single executions used Windows, GNU 16.2 MinGW/C++20 Debug, the
single-owner event engine and seed 42. Wall times include process startup,
simulation and exports. Total time includes all three variants and runner reports.
These are environment-specific observations without confidence intervals or CI
performance thresholds; memory and event rate were not measured.

| Sensors | Nodes | Generated per case | Delivered in reference | Reference process wall s | Three-case runner wall s |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 10 | 14 | 110 | 110 | 0.171 | 0.814 |
| 100 | 104 | 1010 | 1010 | 0.894 | 3.249 |
| 250 | 254 | 2510 | 2012 | 2.707 | 9.190 |

At 250 sensors, offered sensor wire load is 2.5 Mbps before control traffic, above
the reference 2 Mbps uplink; lower delivery is a simulated capacity outcome, not
evidence that the simulator failed. Raw measurements/environment/config/binary
identity remain under `build-core-mingw/industrial-{10,100,250}` in this workspace.
The 128-group Analytics limit can omit per-flow details at larger counts; its
overflow counters remain explicit and the Traffic Lab outcome ledger retains
every configured flow.

At 100 sensors, the primary-uplink outage delivered 810/1010 packets and discarded
200 sensor messages; the alternate path delivered 1010/1010 with 1000 timely sensor
messages. Both networks completed all five control operations. Congestion delivered
534/1010 and only 17 timely sensor messages under either layout, since the selected
delay-based primary route still uses the constrained uplink. This does not establish
an industrial SLA or a general routing/capacity optimum.

Tests execute both actual 100-sensor networks and inspect paths before/during/
after failure, packet counts, control completions and real timing records. MQTT,
gateway/broker node failures, richer device behavior, larger scales and Industrial
Studio controls remain pending.

The optional `--timeout SECONDS` sets a finite positive real-time deadline per
executed case (default 60). For slower machines, e.g. add `--timeout 180` to the
industrial command. This does not change virtual duration, offered traffic or
acceptance criteria. The experiment manifest stores `configuration.timeout_seconds`
and replay preserves it. Invalid/non-finite/nonpositive values fail before
creating scenario outputs. Integration tests use 180 seconds per case to verify
simulation outcomes rather than impose a hardware performance target.
