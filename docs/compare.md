# KNS Compare

Compare saved simulation bundles without rerunning the engine. Profile execution
now generates `comparison.html`, `comparison.json`, paired `comparison.csv`,
long-form `comparison-summary.csv` and `comparison.md`. HTML embeds CSS/SVG and
needs no external chart service or JavaScript. Raw measurements stay in `runs/`.

## Variants and independent topology bundles

```powershell
python scripts/compare_experiments.py results/fault-compare --baseline control
python scripts/compare_experiments.py results/chain --against results/redundant --output "results/topology comparison"
```

`--against` requires a new output directory and preserves both source bundles.
`--baseline` selects the reference variant; `--candidate` selects the candidate.
Each defaults to its bundle's recorded baseline or first variant. A single bundle
can also use `--output <new-directory>` to preserve prior derived reports.
Output version 1.1 retains old delta/count/mean/min/max fields and adds side values,
percentages, variability and direction. Simulator aggregate CSV remains unchanged.

Pairs match seed, routing metric, payload and TCP congestion control. Groups vary
only distinct seeds. Missing settings remain unavailable, never positionally paired
or filled with invented zeros. Failed/transport-failed cases are excluded from
numeric summaries but remain visible; individual metrics have their own counts.
The matrix is bounded to 10000 source cases before expansion.

To compare route-selection policies explicitly, supply both
`--reference-routing delay` and `--candidate-routing bandwidth`. Both are recorded
in pair/group labels; payload, seed and TCP controls still match. This compares
existing Dijkstra cost policies, not an additional routing algorithm.

The bundled topology example keeps endpoint IDs/roles, TCP demand, five seeds,
fault schedule and observation times equal while adding a direct link:

```powershell
python scripts/experiment_profile.py --profile "KNS Experiments/topology-compare/chain-profile.json" --exe build-core-mingw/app/KNS.exe --output results/chain
python scripts/experiment_profile.py --profile "KNS Experiments/topology-compare/redundant-profile.json" --exe build-core-mingw/app/KNS.exe --output results/redundant
python scripts/compare_experiments.py results/chain --against results/redundant --output "results/topology comparison"
```

The direct link changes both redundancy and path delay/hop count. Treat its effect
as a combined configuration change. Across arbitrary bundles, matching CLI settings
alone does not establish equivalent offered traffic: device roles and topology
affect workload. JSON/HTML retain source configurations, revisions, binary hashes
and environment when present. Different executable identities are flagged as
potentially confounding. Inspect inputs; causality is not inferred automatically.

## Deltas and directions

For each seed, `d = candidate - reference`; relative delta is `100*d/reference`,
null for a zero reference. Summary percentages average defined paired percentages,
not the percentage difference between group means. Relative and absolute counts
can differ. Both side means and the contributing sample count remain explicit.

Higher delivery counter ratio/throughput and lower loss counter ratio/DATA latency/
queue overflow are labelled observed improvement; reverse deltas are regression.
Zero is unchanged. Virtual duration and RED early drops are context-dependent.
Intentional RED can increase early drops; virtual duration includes cleanup timers
and observation events. These labels describe metric direction, not statistical
significance, total network quality or deployment performance. There is no hidden
tolerance or pass/fail threshold.

Throughput is delivered packets per final virtual second, including control
packets and timer/observation periods. It is not a physical link capacity measure.
Delivered DATA cohorts can differ after loss, affecting measured latency. Existing
[counter definitions and limitations](experiments.md) continue to apply.

## Variability and conditional intervals

Summaries include the mean, median, extremes and sample standard deviation of
paired deltas (n-1 denominator). These are across seeds, not packet percentiles.
For n >= 2 with nonzero observed variation, the conditional interval is:

`mean(d) ± t(0.975, min(n-1, 30)) * sample_sd(d) / sqrt(n)`.

Critical values use the [NIST Student-t table](https://www.itl.nist.gov/div898/handbook/eda/section3/eda3672.htm),
rounded to three decimals. Capping degrees of freedom at 30 uses a wider critical
value for larger samples. This is an interval of the mean paired delta, not a
prediction interval or interval of individual packet measurements.

Nominal 95% coverage assumes independent, normally distributed paired seed deltas.
The tool does not establish those assumptions. Small samples, skew, successful-pair
selection and uncontrolled workload changes can invalidate interpretation. There
is no multiple-comparison correction. Single pairs give null interval/standard
deviation; constant deltas give null interval with `no_observed_variation`, not a
claim that uncertainty is zero. No causal or significance verdict is generated.

HTML charts display individual deltas, their mean and conditional interval when
available. Each chart uses its own scale around zero; use numeric values across
metrics. Direction colors have text labels and SVG titles carry the summary.
Tables scroll on small screens. Artifact strings are escaped and the content
security policy disables scripts and external assets.

## Remaining scope

Inputs currently support aggregate throughput in packets/s, mean delivered DATA
latency, delivery/loss counter ratios, virtual duration and queue drop counters.
Goodput, packet latency median/p95/p99, jitter, utilization, queue time series,
quantitative resilience and service recovery remain unavailable to this comparator.
Median of seed deltas is not median packet latency. Missing measurements are not
approximated. General traffic models, additional routing algorithms and Studio
comparison UI remain pending.
