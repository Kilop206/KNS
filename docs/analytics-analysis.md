# Delay variation and offline Analytics reports

This extends [runtime Analytics](analytics.md) without changing traffic models,
random streams or the aggregate CSV contract.

## Within-flow latency change

For consecutive DATA deliveries of each tracked flow/direction,
`v_i = abs(latency_i - latency_(i-1))`. The first DATA delivery contributes no pair;
ACKs do not reset history and different flows never form a pair. Retransmissions
and reordering retain delivery order. This is a declared delay-variation measure,
not a protocol-specific interarrival-jitter estimator.

Flow groups and global/window aggregates include variation statistics. Destination
groups do not mix their flows into this measure. Global variation covers tracked
flow groups only; group overflow is explicit. A pair is associated with its second
delivery for window/retention scope. Moments and histogram cover all observed
pairs while quantiles retain recent pairs. Retention does not reset flow history.

## Repeated joint pressure

A pressure window needs complete utilization evidence, busiest-slot fraction
>=0.9 and sampled queue fraction >=0.8. Missing/invalid evidence or a lower value
breaks the streak. Link summaries retain counts/durations and longest streak
independently of time-series trimming.

`consecutive_serialization_queue_pressure` triggers at three consecutive windows.
Its evidence includes virtual span, thresholds and total observed pressure time.
Three windows depend on sampling interval and do not prove sustained overload or
its cause. Queue occupancy still includes propagation; a lone serialization peak
without concurrent queue pressure does not trigger this rule.

## Observed link utilization contrast

Each link summary includes the duration-weighted mean of the busiest-slot
serialization fraction per sample window. Its denominator is valid observed
virtual seconds, excluding undefined windows; zero coverage yields null.
`serialization_utilization_imbalance` compares at least two original tracked
links only when each has complete reservation coverage and valid durations
covering the entire declared horizon (relative floating-point tolerance 1e-10).
It flags a mean >=0.8 alongside a mean <=0.2. Evidence records both link indices,
means, thresholds, comparable count and virtual duration. Scalar integrals survive
time-series trimming. Short peaks, partial runs and unknown coverage do not meet
this full-horizon comparison rule.

This is a utilization contrast, not proof of poor routing, inefficient capacity
allocation or a bottleneck. Different link capacities, endpoint demand and route
roles can explain it. Recommendations ask for controlled experiments and input
inspection; they never automatically modify routes or capacities.

## Export a completed native CLI report

```powershell
python scripts/analytics_report.py "results/analytics/runs/<case>.csv.analytics.json" --output "results/analytics view"
```

The output directory must be new. The source is preserved. Outputs are
`analytics.html`, `series.csv`, `link-series.csv`, `groups.csv`, `histograms.csv`
and `recommendations.md`. HTML embeds charts/histograms and diagnostic evidence
without scripts, external assets, servers or plotting dependencies. Only the
first 16 original links are charted; CSV includes all retained links. Missing
values remain chart gaps/blank cells. Older reports without variation show N/A.
Percentile scopes and counts remain explicit. Input is limited to 64 MiB; larger
native files require lower retention or direct JSON use. Partial reports are
rejected rather than exported as complete observations.

Before batch acceptance or export, checks cover finite values, histogram/count
conservation, retention scopes, moments/percentile bounds, group deliveries and
within-flow pair counts, interval continuity, rate/counter consistency, utilization
bounds and diagnostic explanation fields. Internal consistency is not proof of
authenticity or truth for arbitrary third-party reports.

Tests cover 11/21/6 ms packet latencies yielding 10/15 ms within-flow changes,
exclusion of an interleaved flow, repeated pressure, transient/unknown-coverage
exclusions, actual-engine export, deterministic output, invalid/older reports,
source preservation, HTML escaping and chart gaps. Device processing, application-read
goodput, standard-specific jitter, routing-concentration diagnoses, streaming
storage and Studio Analytics remain pending.
