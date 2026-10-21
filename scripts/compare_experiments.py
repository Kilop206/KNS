#!/usr/bin/env python3
"""Compare completed experiment variants with a baseline, without rerunning KNS."""
from __future__ import annotations

import argparse
import csv
import itertools
import json
import math
from pathlib import Path, PureWindowsPath
import re
import statistics
import sys


DIMENSIONS = ("routing_metric", "seed", "packet_size", "congestion_control")
METRICS = ("delivery_rate", "loss_rate", "throughput_pps", "avg_latency_s",
           "simulation_duration_s", "queue_overflow_drops", "red_early_drops")
DEFAULTS = {"routing_metrics": ["delay"], "seeds": [42], "packet_sizes": [1500],
            "congestion_controls": ["reno"]}


def build_comparison(profile: dict, benchmark: dict, baseline: str | None = None) -> dict:
    """Pair only identical workload settings; retain failed and missing pairs."""
    if not isinstance(profile, dict) or not isinstance(benchmark, dict):
        raise ValueError("Profile and benchmark must be JSON objects")
    if benchmark.get("schema_version") != "1.0":
        raise ValueError("Unsupported benchmark schema_version")
    experiments = profile.get("experiments")
    if not isinstance(experiments, list) or not experiments:
        raise ValueError("Profile must contain experiments")
    names = []
    for experiment in experiments:
        name = experiment.get("name") if isinstance(experiment, dict) else None
        if not isinstance(name, str) or re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", name) is None:
            raise ValueError("Invalid experiment name")
        names.append(name)
    if len({name.casefold() for name in names}) != len(names):
        raise ValueError("Experiment names must be unique")
    baseline = baseline if baseline is not None else profile.get("baseline", names[0])
    if baseline not in names:
        raise ValueError("Baseline must name an experiment in the profile")
    suite = benchmark.get("suite")
    if not isinstance(suite, dict) or suite.get("name") != profile.get("name"):
        raise ValueError("Benchmark suite does not match the profile")
    paths = suite.get("topologies")
    if not isinstance(paths, list) or len(paths) != len(names):
        raise ValueError("Benchmark topologies do not match the profile variants")
    for index, (name, path) in enumerate(zip(names, paths), start=1):
        if not isinstance(path, str) or PureWindowsPath(path).name != f"{index:03d}-{name}.json":
            raise ValueError("Benchmark topology order does not match the generated variants")
    for field, default in DEFAULTS.items():
        values = profile.get(field, default)
        if not isinstance(values, list) or not values or suite.get(field) != values:
            raise ValueError(f"Benchmark {field} do not match the profile")
        for value in values:
            if field == "seeds":
                valid = type(value) is int and 0 <= value <= 2**64 - 1
            elif field == "packet_sizes":
                valid = type(value) is int and value > 0
            else:
                choices = ({"delay", "bandwidth", "hop-count", "delay-bandwidth"}
                           if field == "routing_metrics" else {"tahoe", "reno", "newreno", "cubic"})
                valid = isinstance(value, str) and value in choices
            if not valid:
                raise ValueError(f"Invalid profile {field}")
        if len(set(values)) != len(values):
            raise ValueError(f"Profile {field} must contain distinct values")
    if suite.get("link_event_scenarios") != [{"name": "baseline", "events": []}]:
        raise ValueError("Profile comparisons require the generated no-fault scenario")

    settings = list(itertools.product(*(profile.get(field, default)
                                       for field, default in DEFAULTS.items())))
    expected = {(path, *setting) for path in paths for setting in settings}
    records = benchmark.get("cases")
    if not isinstance(records, list):
        raise ValueError("Benchmark must contain a cases list")
    indexed = {}
    for row in records:
        if not isinstance(row, dict):
            raise ValueError("Benchmark case must be an object")
        try:
            key = (row["topology"], *(row[field] for field in DIMENSIONS))
            if type(row["seed"]) is not int or type(row["packet_size"]) is not int:
                raise ValueError("Benchmark seed and packet_size must be integers")
            if key not in expected or row.get("fault_scenario") != "baseline" or row.get("link_events") != []:
                raise ValueError("Benchmark contains a case outside the profile matrix")
            if key in indexed:
                raise ValueError("Benchmark contains duplicate cases for the same workload")
            indexed[key] = row
        except (KeyError, TypeError) as exc:
            raise ValueError("Benchmark case has invalid workload settings") from exc

    def available(row: dict | None) -> bool:
        return bool(row and row.get("status") == "success" and type(row.get("returncode")) is int
                    and row["returncode"] == 0)

    def metric_delta(candidate: dict, control: dict, field: str) -> float | None:
        left, right = candidate.get(field), control.get(field)
        if left is None or right is None:
            return None
        if any(type(value) not in (int, float) or not math.isfinite(value) or value < 0
               for value in (left, right)):
            raise ValueError(f"Invalid metric {field}; expected finite nonnegative numbers or null")
        return left - right

    pairs = []
    baseline_path = paths[names.index(baseline)]
    for name, path in zip(names, paths):
        if name == baseline:
            continue
        for setting in settings:
            control = indexed.get((baseline_path, *setting))
            candidate = indexed.get((path, *setting))
            matched = available(control) and available(candidate)
            pairs.append({
                "experiment": name, "baseline": baseline,
                **dict(zip(DIMENSIONS, setting)),
                "status": "matched" if matched else "unavailable",
                "baseline_status": control.get("status", "unknown") if control else "missing",
                "experiment_status": candidate.get("status", "unknown") if candidate else "missing",
                "baseline_returncode": control.get("returncode") if control else None,
                "experiment_returncode": candidate.get("returncode") if candidate else None,
                "baseline_case_id": control.get("case_id") if control else None,
                "experiment_case_id": candidate.get("case_id") if candidate else None,
                **{f"{field}_delta": metric_delta(candidate, control, field) if matched else None
                   for field in METRICS},
            })

    groups = {}
    for pair in pairs:
        key = tuple(pair[field] for field in ("experiment", "routing_metric", "packet_size", "congestion_control"))
        groups.setdefault(key, []).append(pair)
    summaries = []
    for key, group in groups.items():
        metric_summaries = {}
        for field in METRICS:
            values = [pair[f"{field}_delta"] for pair in group if pair[f"{field}_delta"] is not None]
            metric_summaries[field] = {
                "sample_count": len(values),
                "mean_delta": statistics.fmean(values) if values else None,
                "min_delta": min(values) if values else None,
                "max_delta": max(values) if values else None,
            }
        matched_count = sum(pair["status"] == "matched" for pair in group)
        summaries.append({
            **dict(zip(("experiment", "routing_metric", "packet_size", "congestion_control"), key)),
            "expected_pairs": len(group), "matched_pairs": matched_count,
            "unavailable_pairs": len(group) - matched_count, "metrics": metric_summaries,
        })
    return {
        "schema_version": "1.0", "profile": profile["name"], "baseline": baseline,
        "comparison": "experiment-minus-baseline", "kns_commit": benchmark.get("kns_commit"),
        "expected_pairs": len(pairs),
        "matched_pairs": sum(pair["status"] == "matched" for pair in pairs),
        "unavailable_pairs": sum(pair["status"] != "matched" for pair in pairs),
        "summaries": summaries, "pairs": pairs,
    }


def write_reports(comparison: dict, output: Path) -> None:
    (output / "comparison.json").write_text(
        json.dumps(comparison, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    fields = ["experiment", "baseline", *DIMENSIONS, "status", "baseline_status", "experiment_status",
              "baseline_returncode", "experiment_returncode", "baseline_case_id", "experiment_case_id",
              *(f"{field}_delta" for field in METRICS)]
    with (output / "comparison.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(comparison["pairs"])

    lines = [f"# Experiment comparison: {comparison['profile']}", "",
             f"Baseline: `{comparison['baseline']}`. All deltas are experiment minus baseline.", "",
             f"Matched pairs: {comparison['matched_pairs']}/{comparison['expected_pairs']}. "
             f"Unavailable pairs: {comparison['unavailable_pairs']}.", "",
             "Cases are paired by seed, routing metric, payload size and congestion control. "
             "Only successful cases on both sides enter numeric summaries.", "",
             "Positive latency or duration deltas mean an increase; negative throughput deltas mean a decrease. "
             "Delivery/loss deltas are differences between aggregate counter ratios.", "",
             "## Mean paired deltas across seeds", "",
             "Each metric cell shows mean delta (number of contributing pairs). "
             "Undefined metrics remain N/A; JSON also records each delta's minimum and maximum.", "",
             "| Experiment | Routing | Payload bytes | TCP | Matched / expected | "
             "Delivery | Loss | Throughput pps | Latency s | Duration s | Queue overflow | RED early |",
             "| --- | --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for summary in comparison["summaries"]:
        cells = [summary["experiment"], summary["routing_metric"], str(summary["packet_size"]),
                 summary["congestion_control"], f"{summary['matched_pairs']}/{summary['expected_pairs']}"]
        for field in METRICS:
            metric = summary["metrics"][field]
            cells.append(f"{metric['mean_delta']:+.6g} ({metric['sample_count']})"
                         if metric["mean_delta"] is not None else "N/A (0)")
        lines.append("| " + " | ".join(cells) + " |")
    unavailable = [pair for pair in comparison["pairs"] if pair["status"] != "matched"]
    if unavailable:
        lines += ["", "## Unavailable pairs", ""]
        for pair in unavailable:
            lines.append(f"- `{pair['experiment']}`, seed {pair['seed']}, {pair['routing_metric']}, "
                         f"{pair['packet_size']} bytes, {pair['congestion_control']}: "
                         f"baseline `{pair['baseline_status']}` (exit {pair['baseline_returncode']}), "
                         f"experiment `{pair['experiment_status']}` (exit {pair['experiment_returncode']}).")
    lines += ["", "Raw statistics, commands and process diagnostics remain in `runs/benchmark.json` "
              "and the per-case CSV/log files."]
    (output / "comparison.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def compare_artifacts(directory: Path, baseline: str | None = None) -> dict:
    # Use recorded inputs only: analysis also works after moving an artifact folder.
    profile = json.loads((directory / "profile.json").read_text(encoding="utf-8"))
    benchmark = json.loads((directory / "runs" / "benchmark.json").read_text(encoding="utf-8"))
    try:
        comparison = build_comparison(profile, benchmark, baseline)
    except (KeyError, TypeError, OverflowError) as exc:
        raise ValueError(f"Invalid comparison artifacts: {exc}") from exc
    write_reports(comparison, directory)
    return comparison


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Experiment artifact directory containing profile.json and runs/")
    parser.add_argument("--baseline", help="Override the reference experiment without rerunning KNS")
    args = parser.parse_args(argv)
    try:
        directory = args.directory.resolve()
        comparison = compare_artifacts(directory, args.baseline)
    except (OSError, ValueError) as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1
    print(f"Comparison: {directory / 'comparison.md'} "
          f"({comparison['matched_pairs']}/{comparison['expected_pairs']} matched pairs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
