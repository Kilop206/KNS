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
Partial/budget-truncated reports are rejected. Unknown flow IDs are input errors.

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
use virtual metrics, without wall-clock performance thresholds. Workflow execution
on remote Linux/macOS/MinGW runners still needs verification in GitHub Actions.

This is an initial validation subsystem. Scenario-embedded rules, automatic
baseline-relative thresholds, richer assertion languages and Studio validation
remain pending. The Python companion CLI does not replace the planned native
`kns validate` subcommand.
