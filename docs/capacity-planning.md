# Observed capacity alternatives

`scripts/capacity_planner.py` executes explicit sensor-population and uplink
alternatives using the Industrial/Traffic/Fault/Analytics models. Every row comes
from an actual complete simulation, with its own retained report and per-population
manifest. It does not extrapolate a network's maximum capacity from a few samples.

```powershell
python scripts/capacity_planner.py --exe build-core-mingw/verification-build/app/KNS.exe --output results/capacity --base-sensors 100 --growth-percent 0 10 25 50 100 --bandwidth-mbps 2 4
```

Growth percentages are relative to the base population and round upward using
decimal arithmetic. Thus 100 with 10% growth is 110, including the usual binary
floating-point edge case. Counts stay within the Industrial generator's 1..250
range. Up to 20 growth levels and 20 bandwidth alternatives are accepted, subject
to `--max-cases` (default 32, hard limit 100). Duplicate populations/alternatives
and excessive matrices fail before creating output.

Sources retain the declared one-second telemetry/control workload and deadlines.
Defaults require packet delivery >=0.99 and timely sensor delivery >=0.99;
`--min-delivery` and `--min-sensor-on-time` change these explicit engineering
requirements. They are not certified industrial SLAs. `--redundant` adds the same
alternate uplink path as the Industrial scenario. `--with-outage` applies the
same timed primary-uplink fault to every alternative. Compare populations
separately; their endpoint counts and offered loads differ by design.

Outputs: `configuration.json`, `capacity.json`, `capacity.csv`, `capacity.md` and
each `population-<count>/experiment` with complete raw metrics/manifests. Rows
include generated/delivered/dropped counts, generated-packet delivery ratio,
timely sensor deliveries divided by generated sensor packets, all-DATA mean
network latency in seconds, requirement verdict, measured wall time and evidence
paths. Timing includes process/export overhead and is not an acceptance threshold.
Root summaries report only the highest **observed** feasible population per link
alternative. Higher populations can remain untested and monotonicity is not assumed.

## Optional user costs and Pareto frontier

Supply `--costs user-costs.json` only when prices are available. The versioned
contract `contracts/capacity-costs-v1.schema.json` requires `currency` and an
`uplinks` array of `{bandwidth_mbps, cost}`, covering exactly the evaluated
alternatives. An optional `sensor_unit_cost` adds the user's per-sensor amount.
Duplicate/missing costs, negative/non-finite numbers and oversized input fail
preflight. No currency, device price, upgrade price or free infrastructure is
assumed when the file is absent.

The reported numeric cost is the declared uplink alternative plus the optional
sensor component. Other devices, redundancy, labor and operating costs are
excluded unless already included in the user's alternative amount; it is not
total ownership cost or an infrastructure quote. Without prices, costs and
Pareto flags remain unavailable. With complete prices, each population gets a
frontier minimizing declared cost and mean DATA network latency while maximizing
delivery and timely sensor fractions. Equal alternatives remain coequal. This
frontier reflects those metrics only and never selects a global winner.

## Executed reference and limitations

A measured four-case example under `build-core-mingw/capacity-final` used 100/200
sensors, 1/4 Mbps and the default requirements, without prices:

| Sensors | Mbps | Generated | Delivered | Dropped | Timely sensor ratio | Meets requirements |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 100 | 1 | 1010 | 1010 | 0 | 1.000 | yes |
| 100 | 4 | 1010 | 1010 | 0 | 1.000 | yes |
| 200 | 1 | 2010 | 1026 | 984 | 0.359 | no |
| 200 | 4 | 2010 | 2010 | 0 | 1.000 | yes |

The highest observed feasible populations are 100 and 200 respectively, not
proven capacity ceilings. This finite workload does not establish steady-state
queue stability, hardware processing capacity, MQTT behavior, real service
performance or behavior beyond the tested populations. Source/binary/environment
identity is in each manifest; raw experiments can be replayed independently.
Tests use actual simulations for growth/bandwidth/resilience and explicit arbitrary
cost units for arithmetic/frontier checks, never fabricated market prices.

New segments/application growth, broader cost models, capacity search strategies,
statistical replication, Studio planning and larger Industrial populations remain
pending. The existing general link/traffic sweep tool remains available for
workloads outside this initial Industrial planning template.
