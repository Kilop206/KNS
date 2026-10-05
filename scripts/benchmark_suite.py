#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import itertools
import math
import json
import os
import platform
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import run as runner


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SUITE = ROOT / "benchmarks" / "v1" / "routing-baseline.json"


def load_suite(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema_version") != "1.0":
        raise ValueError("Unsupported benchmark suite schema_version")
    required = {"name", "topologies", "routing_metrics", "seeds", "packet_sizes"}
    missing = required - set(data)
    if missing:
        raise ValueError(f"Benchmark suite missing fields: {sorted(missing)}")
    if not data["topologies"] or not data["routing_metrics"] or not data["seeds"] or not data["packet_sizes"]:
        raise ValueError("Benchmark suite dimensions must be non-empty")
    valid_metrics = {"delay", "bandwidth", "hop-count", "delay-bandwidth"}
    if not set(data["routing_metrics"]).issubset(valid_metrics):
        raise ValueError("Benchmark suite contains an unsupported routing metric")
    if any(not isinstance(seed, int) or seed < 0 or seed > 2**64 - 1 for seed in data["seeds"]):
        raise ValueError("Benchmark seeds must fit unsigned 64-bit integers")
    if any(not isinstance(size, int) or size <= 0 for size in data["packet_sizes"]):
        raise ValueError("Benchmark packet sizes must be positive integers")
    controls = data.get("congestion_controls", ["reno"])
    valid_controls = {"tahoe", "reno", "newreno", "cubic"}
    if not controls or not set(controls).issubset(valid_controls):
        raise ValueError("Benchmark suite contains an unsupported congestion control")
    data["congestion_controls"] = controls

    scenarios = data.get("link_event_scenarios", [{"name": "baseline", "events": []}])
    if not isinstance(scenarios, list) or not scenarios:
        raise ValueError("Benchmark link_event_scenarios must be a non-empty list")
    seen_scenarios = set()
    for scenario in scenarios:
        if not isinstance(scenario, dict) or set(scenario) != {"name", "events"}:
            raise ValueError("Each link event scenario must contain exactly name and events")
        name = scenario["name"]
        events = scenario["events"]
        if not isinstance(name, str) or not name or any(
            not (ch.isalnum() or ch in "-_") for ch in name
        ):
            raise ValueError("Benchmark link event scenario name is invalid")
        if name in seen_scenarios:
            raise ValueError("Duplicate benchmark link event scenario")
        seen_scenarios.add(name)
        if not isinstance(events, list):
            raise ValueError("Benchmark link event scenario events must be a list")
        for event in events:
            if not isinstance(event, dict) or set(event) != {"time", "from", "to", "state"}:
                raise ValueError("Link events must contain exactly time, from, to and state")
            if not isinstance(event["time"], (int, float)) or not math.isfinite(event["time"]) or event["time"] < 0:
                raise ValueError("Link event time must be finite and nonnegative")
            if not isinstance(event["from"], int) or not isinstance(event["to"], int) or event["from"] < 0 or event["to"] < 0:
                raise ValueError("Link event endpoints must be nonnegative integers")
            if event["from"] == event["to"] or event["state"] not in {"down", "up"}:
                raise ValueError("Link event endpoints/state are invalid")
    data["link_event_scenarios"] = scenarios

    for topology in data["topologies"]:
        path_value = ROOT / topology
        if not path_value.is_file():
            raise ValueError(f"Benchmark topology does not exist: {topology}")
    return data


def expand_cases(suite: dict) -> list[dict]:
    cases = []
    for topology, metric, seed, packet_size, congestion_control, scenario in itertools.product(
        suite["topologies"],
        suite["routing_metrics"],
        suite["seeds"],
        suite["packet_sizes"],
        suite["congestion_controls"],
        suite["link_event_scenarios"],
    ):
        cases.append({
            "topology": topology,
            "routing_metric": metric,
            "seed": seed,
            "packet_size": packet_size,
            "congestion_control": congestion_control,
            "fault_scenario": scenario["name"],
            "link_events": [
                f'{event["time"]}:{event["from"]}:{event["to"]}:{event["state"]}'
                for event in scenario["events"]
            ],
        })
    return cases


def git_commit() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=ROOT,
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except Exception:
        return "unknown"


def safe_case_id(case: dict, index: int) -> str:
    stem = Path(case["topology"]).stem
    metric = case["routing_metric"].replace("-", "_")
    return (
        f"{index:03d}-{stem}-{metric}-{case['congestion_control']}-"
        f"{case['fault_scenario']}-s{case['seed']}-p{case['packet_size']}"
    )


def write_metrics(rows: list[dict], out: Path) -> None:
    fields = [
        "case_id",
        "topology",
        "routing_metric",
        "seed",
        "packet_size",
        "congestion_control",
        "fault_scenario",
        "status",
        "returncode",
        "wall_clock_duration_s",
        "simulation_duration_s",
        "packets_sent",
        "packets_delivered",
        "packets_lost",
        "delivery_rate",
        "loss_rate",
        "throughput_pps",
        "avg_latency_s",
    ]
    with out.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)



def comparison_key(row: dict) -> tuple:
    return (
        row["topology"],
        row["routing_metric"],
        row["seed"],
        row["packet_size"],
        row["congestion_control"],
    )


def compare_fault_scenarios(rows: list[dict]) -> list[dict]:
    baseline = {
        comparison_key(row): row
        for row in rows
        if row.get("fault_scenario") == "baseline" and row.get("returncode") == 0
    }
    comparisons = []
    for row in rows:
        scenario = row.get("fault_scenario")
        if scenario == "baseline" or row.get("returncode") != 0:
            continue
        control = baseline.get(comparison_key(row))
        if control is None:
            continue

        def delta(field: str):
            left, right = row.get(field), control.get(field)
            if left is None or right is None:
                return None
            return left - right

        comparisons.append({
            "topology": row["topology"],
            "routing_metric": row["routing_metric"],
            "seed": row["seed"],
            "packet_size": row["packet_size"],
            "congestion_control": row["congestion_control"],
            "fault_scenario": scenario,
            "baseline_case_id": control["case_id"],
            "fault_case_id": row["case_id"],
            "delivery_rate_delta": delta("delivery_rate"),
            "loss_rate_delta": delta("loss_rate"),
            "throughput_pps_delta": delta("throughput_pps"),
            "avg_latency_s_delta": delta("avg_latency_s"),
            "simulation_duration_s_delta": delta("simulation_duration_s"),
        })
    return comparisons


def write_comparisons(rows: list[dict], output: Path) -> None:
    comparisons = compare_fault_scenarios(rows)
    payload = {
        "schema_version": "1.0",
        "comparison": "fault-minus-baseline",
        "case_count": len(comparisons),
        "cases": comparisons,
    }
    (output / "scenario-comparison.json").write_text(
        json.dumps(payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    fields = [
        "topology",
        "routing_metric",
        "seed",
        "packet_size",
        "congestion_control",
        "fault_scenario",
        "baseline_case_id",
        "fault_case_id",
        "delivery_rate_delta",
        "loss_rate_delta",
        "throughput_pps_delta",
        "avg_latency_s_delta",
        "simulation_duration_s_delta",
    ]
    with (output / "scenario-comparison.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(comparisons)



def _format_number(value, digits: int = 4) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, int):
        return str(value)
    return f"{value:.{digits}f}"


def write_markdown_report(metadata: dict, output: Path, comparisons: list[dict]) -> None:
    rows = metadata["cases"]
    successful = [row for row in rows if row.get("returncode") == 0]
    lines = [
        f"# {metadata['suite']['name']}",
        "",
        f"- KNS version: `{metadata['kns_version']}`",
        f"- KNS commit: `{metadata['kns_commit']}`",
        f"- Generated: {metadata['generated_at']}",
        f"- Platform: {metadata['platform']}",
        f"- Cases: {metadata['case_count']} total, {metadata['successful_cases']} successful, {metadata['failed_cases']} failed",
        "",
        "## Aggregate metrics",
        "",
        "| Metric | Mean across successful cases |",
        "| --- | ---: |",
    ]

    aggregates = [
        ("Delivery rate", "delivery_rate"),
        ("Loss rate", "loss_rate"),
        ("Throughput (pps)", "throughput_pps"),
        ("Average latency (s)", "avg_latency_s"),
        ("Simulation duration (s)", "simulation_duration_s"),
    ]
    for label, field in aggregates:
        values = [row[field] for row in successful if row.get(field) is not None]
        mean = sum(values) / len(values) if values else None
        lines.append(f"| {label} | {_format_number(mean)} |")

    lines += ["", "## Cases", "", "| Case | Status | Delivery | Loss | Throughput pps | Latency s | Sim duration s |", "| --- | --- | ---: | ---: | ---: | ---: | ---: |"]
    for row in rows:
        lines.append(
            "| {case} | {status} | {delivery} | {loss} | {throughput} | {latency} | {duration} |".format(
                case=row["case_id"],
                status=row["status"],
                delivery=_format_number(row.get("delivery_rate")),
                loss=_format_number(row.get("loss_rate")),
                throughput=_format_number(row.get("throughput_pps")),
                latency=_format_number(row.get("avg_latency_s")),
                duration=_format_number(row.get("simulation_duration_s")),
            )
        )

    if comparisons:
        lines += [
            "",
            "## Fault vs baseline",
            "",
            "All deltas are fault scenario minus the matching baseline.",
            "",
            "| Scenario | Seed | Delivery Δ | Loss Δ | Throughput Δ | Latency Δ s | Duration Δ s |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
        ]
        for row in comparisons:
            lines.append(
                "| {scenario} | {seed} | {delivery} | {loss} | {throughput} | {latency} | {duration} |".format(
                    scenario=row["fault_scenario"],
                    seed=row["seed"],
                    delivery=_format_number(row.get("delivery_rate_delta")),
                    loss=_format_number(row.get("loss_rate_delta")),
                    throughput=_format_number(row.get("throughput_pps_delta")),
                    latency=_format_number(row.get("avg_latency_s_delta")),
                    duration=_format_number(row.get("simulation_duration_s_delta")),
                )
            )

    if metadata["failed_cases"]:
        lines += [
            "",
            "## Failures",
            "",
            "Failed cases are retained as evidence and are excluded from numeric aggregates/comparisons.",
            "",
        ]
        for row in rows:
            if row.get("returncode") != 0:
                lines.append(f"- `{row['case_id']}`: {row['status']} (return code {row['returncode']})")

    (output / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run a versioned KNS benchmark suite")
    parser.add_argument("--suite", type=Path, default=DEFAULT_SUITE)
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument("--exe", type=Path, default=None)
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    try:
        suite = load_suite(args.suite.resolve())
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"[ERROR] Invalid benchmark suite: {exc}", file=sys.stderr)
        return 1

    cases = expand_cases(suite)
    if args.dry_run:
        print(
            json.dumps(
                {
                    "suite": suite["name"],
                    "case_count": len(cases),
                    "cases": cases,
                },
                indent=2,
            )
        )
        return 0

    if args.timeout <= 0:
        print("[ERROR] --timeout must be positive", file=sys.stderr)
        return 1

    exe = args.exe.resolve() if args.exe else runner.find_executable(ROOT)
    if not exe or not exe.is_file():
        print("[ERROR] KNS executable not found; build KNS or pass --exe", file=sys.stderr)
        return 1

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = (args.output or ROOT / "results" / "benchmarks" / suite["name"] / timestamp).resolve()
    output.mkdir(parents=True, exist_ok=False)

    records: list[dict] = []
    environment = {**os.environ, "KNS_AUTO_START": "1"}

    for index, case in enumerate(cases, start=1):
        case_id = safe_case_id(case, index)
        csv_path = output / f"{case_id}.csv"
        log_path = output / f"{case_id}.log"
        topology = (ROOT / case["topology"]).resolve()
        command = runner.build_command(
            exe,
            topology,
            csv_path,
            case["routing_metric"],
            case["seed"],
            case["packet_size"],
            case["congestion_control"],
            case["link_events"],
        )

        started = time.perf_counter()
        status = "success"
        returncode = 0
        error = None
        with log_path.open("w", encoding="utf-8") as log:
            try:
                completed = subprocess.run(
                    command,
                    cwd=topology.parent,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    stdin=subprocess.DEVNULL,
                    timeout=args.timeout,
                    env=environment,
                    check=False,
                )
                returncode = completed.returncode
                if returncode != 0:
                    status = "process_error"
            except subprocess.TimeoutExpired:
                status = "timeout"
                returncode = 124

        wall = time.perf_counter() - started
        stats = None
        if returncode == 0:
            try:
                stats = runner.parse_stats(csv_path)
            except (OSError, ValueError) as exc:
                status = "stats_error"
                returncode = 1
                error = str(exc)

        derived = runner.compute_stats(stats, wall)
        records.append({
            "case_id": case_id,
            **case,
            "status": status,
            "returncode": returncode,
            "error": error,
            "command": command,
            "log": str(log_path),
            "csv": str(csv_path) if csv_path.exists() else None,
            "wall_clock_duration_s": derived["wall_clock_duration_s"],
            "simulation_duration_s": derived["simulation_duration_s"],
            "packets_sent": derived["packets_sent"],
            "packets_delivered": derived["packets_delivered"],
            "packets_lost": derived["packets_lost"],
            "delivery_rate": derived["delivery_rate"],
            "loss_rate": derived["loss_rate"],
            "throughput_pps": derived["throughput_pps"],
            "avg_latency_s": derived["latency_mean_s"],
        })
        print(f"[{index}/{len(cases)}] {case_id}: {status}")

    metadata = {
        "schema_version": "1.0",
        "suite": suite,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "kns_commit": git_commit(),
        "kns_version": "1.1.0",
        "executable": str(exe),
        "platform": platform.platform(),
        "timeout_seconds": args.timeout,
        "case_count": len(records),
        "successful_cases": sum(1 for row in records if row["returncode"] == 0),
        "failed_cases": sum(1 for row in records if row["returncode"] != 0),
        "cases": records,
    }
    (output / "benchmark.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    write_metrics(records, output / "metrics.csv")
    comparisons = (
        compare_fault_scenarios(records)
        if len(suite["link_event_scenarios"]) > 1
        else []
    )
    if len(suite["link_event_scenarios"]) > 1:
        write_comparisons(records, output)
    write_markdown_report(metadata, output, comparisons)
    (output / "suite.json").write_text(
        json.dumps(suite, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    return 1 if metadata["failed_cases"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
