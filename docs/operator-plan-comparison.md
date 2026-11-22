# Compare Network Operator plans offline

Compare two saved `operator-plan-v1` files before deciding whether to execute an
operation. This tool validates both inputs and compares their JSON content; it
never contacts a device or executes the rendered SSH/HTTPS steps.

```powershell
python -m pip install -r scripts/requirements-contracts.txt
python scripts/compare_operator_plans.py before.json after.json --output review
python scripts/validate_contracts.py --operator-comparison review/comparison.json
```

Use an output directory that does not exist. The command creates
`comparison.json` and a self-contained `report.html` with escaped text and no
scripts or external resources. Reports contain before/after plan values, so
handle them with the same care as their input files.

Exit codes are **0** for equivalent plans, **2** for different plans, and **1**
for invalid inputs, arguments, missing dependencies or output errors. File hashes
identify the original bytes: whitespace/key order can produce different hashes
while the plans remain equivalent. The output is review evidence, not permission
to execute a plan.

Comparison ignores object key order, preserves array order, distinguishes
missing values from `null` and booleans from numbers, and treats `1` and `1.0`
as equivalent. Integer literals retain Python's exact integer precision;
fractional/exponent literals use standard Python floating-point parsing.
Differences use escaped JSON pointers, in sorted key/index order. Replacing a
container with a different JSON type counts as one difference at that path;
two containers of the same type are compared recursively. An empty pointer
identifies the root. `before` is absent for additions and `after` for removals.

Inputs are limited to 4 MiB each and 64 container levels. Duplicate keys,
nonfinite numbers, invalid UTF-8 and invalid plans are rejected before output
creation. `--max-changes` defaults to 200 and accepts 1–1000; it limits recorded
differences, while `change_count` still counts every difference. `truncated`
indicates an incomplete list. Neither input is modified and existing output is
never overwritten.

The comparison contract is `contracts/operator-plan-comparison-v1.schema.json`.
Consumers should also verify that `reported_changes` equals the list length,
does not exceed `change_count`, and that `truncated` matches those counts;
these relationships cannot be expressed by this JSON Schema alone.
