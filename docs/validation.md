# Validate complete simulated workload results

`scripts/validate_result.py` checks explicit JSON assertions against a complete
Traffic Lab report without running the simulator again. It validates the report's
plan, packet/byte/stage/operation conservation and completion before evaluating
rules. Input consistency is not authentication or validation against real networks.

```powershell
python scripts/validate_result.py "results/traffic-experiment/runs/003-002-upgrade-delay-reno-baseline-s42-p1500.csv.traffic.json" --rules "KNS Experiments/traffic-lab/assertions.json" --output "results/traffic verdict"
```

The new output directory contains `validation.json`, JUnit `validation.xml` and
`validation.md`. Sources and previous results are preserved. JSON records source,
rule, configuration and validator-source SHA-256 identities, observed/expected values, units,
denominator/retention scope and individual pass/fail reasons. Exit codes are 0
when every assertion passes, 1 for threshold/unavailable-metric failures and 2
for invalid input or output errors. Empty populations never become an invented
zero or a passing assertion, including comparisons using `!=`.

Rules use schema version 1.0 and 1..1024 assertions with `metric`, `operator`,
finite numeric `value`, optional unique `name` and optional numeric `flow_id`.
Supported operators are `>=`, `<=`, `>`, `<`, `==`, `!=`. Ratios are fractions;
latencies use seconds. Source reports are bounded to 64 MiB and rules to 1 MiB.
Partial/budget-truncated reports are rejected. Unknown flow IDs and duplicate JSON fields are input errors.

| Metric | Population/denominator |
| --- | --- |
| `generated`, `delivered`, `dropped`, `pending` | Scalar packet counts over selected flows |
| `operations_started`, `operations_completed`, `operations_failed`, `operations_pending` | Request/response operation counts over selected flows |
| `delivery_ratio`, `packet_loss_ratio`, `pending_ratio` | Delivered/dropped/pending packets divided by generated packets |
| `operation_completion_ratio` | Completed operations divided by started operations |
| `on_time_operation_ratio` | On-time completions divided by started operations in flows with operation deadlines; failures remain in denominator |
| `on_time_packet_ratio` | On-time packets divided by generated packets in flows with per-message deadlines |
| `operation_latency_mean_s` | All completed operations of one required `flow_id` |
| `operation_latency_p50_s`, `operation_latency_p95_s`, `operation_latency_p99_s` | Declared retained-sample percentiles of one required `flow_id` |

Per-flow percentiles are never averaged into a global percentile. Count/retention
evidence accompanies latency assertions; add minimum completion-count assertions
when a few observations cannot support a decision. Existing automatic TCP CSV
ratios are not silently substituted for generated-packet ratios.

The example requires delivery >=0.9 and no pending packets. On the deterministic
shared-backbone workload the 10 Mbps variant passes; reducing bandwidth to 1 Mbps
violates the delivery target. Integration tests execute both real scenarios and
verify process codes, JSON and JUnit failures. CI runs the upgraded reference and
publishes experiment/verdict artifacts even after assertion failure. These checks
use virtual metrics, without wall-clock performance thresholds. Earlier build/test/absolute-validation workflows passed on Linux/macOS/MinGW;
the new baseline CI step requires its own remote verification (see implementation status).

This is an initial validation subsystem. Scenario-embedded rules, relative-percent
tolerances, richer assertion languages and Studio validation remain pending. The Python companion CLI does not replace the planned native
`kns validate` subcommand.

## Version 1.1 baseline offsets

Rules 1.0 retain their absolute `value` contract. Version 1.1 supports exactly one
of `value` or `baseline_offset` per assertion. An offset assertion evaluates:

```text
candidate metric  operator  (baseline metric + baseline_offset)
```

Offsets have the metric's unit. For a ratio, -0.01 permits a decrease of one
percentage point; it is not a relative 1% multiplier. For operation latency,
0.005 adds five milliseconds to the baseline's seconds-valued limit. Counts use
count units. All thresholds must remain finite. No epsilon, statistical interval
or automatic causal interpretation is added.

```powershell
python scripts/experiment_profile.py --profile "KNS Experiments/traffic-lab/profile.json" --exe build/app/KNS.exe --output results/traffic-experiment
python scripts/validate_result.py results/traffic-experiment/runs/003-002-upgrade-delay-reno-baseline-s42-p1500.csv.traffic.json --baseline results/traffic-experiment/runs/001-001-control-delay-reno-baseline-s42-p1500.csv.traffic.json --rules "KNS Experiments/traffic-lab/baseline-assertions.json" --output results/baseline-verdict
```

Run the documented Traffic Lab profile first. The example checks the 10 Mbps
upgrade against the actual 1 Mbps control under seed 42 and the same offered
workload. Swapping candidate and baseline tests whether the constrained network
preserves the upgrade's delivery and produces a regression verdict. This is a
controlled reference variant, not a historical performance baseline. A saved
report from a prior compatible run can also serve as the reference.

Both reports must be complete and individually conserve outcomes. Canonical
workload hashes (including seed, ordered flows, sizes, schedules/replay times and
application settings), reported model descriptions, per-flow random stream seeds
and primary generated counts must match. Response/delivery populations can differ
because modeled losses affect causal responses. These checks do not establish
topology/software equivalence or authenticate reports; retain experiment manifests.
Changing processing timers or source settings is a different workload under this
strict interface. Compare such experiments explicitly instead of silently pairing.

Only assertions with baseline_offset require --baseline; an unused reference is
an input error. Missing baseline metrics, empty populations and zero denominators
fail the assertion even for `!=`, with null expected/reference values and an
explicit reason. Different units/percentile scopes are input errors. Percentiles
still require one flow ID; pair latency checks with loss/completion/minimum-count
checks to avoid treating a selectively delivered sample as a service guarantee.

Version-1.1 JSON records baseline file/configuration hashes, baseline observations,
units, scopes, retention/denominator evidence and effective thresholds. JUnit
includes the reference details and Markdown adds a baseline-threshold table.
The existing 0/1/2 exit codes and preservation of existing outputs apply. See
[the rules 1.1 contract](../contracts/validation-rules-v1.1.schema.json).
