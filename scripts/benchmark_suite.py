#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import itertools
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
    for topology in data["topologies"]:
        path_value = ROOT / topology
        if not path_value.is_file():
            raise ValueError(f"Benchmark topology does not exist: {topology}")
    return data


def expand_cases(suite: dict) -> list[dict]:
    cases = []
    for topology, metric, seed, packet_size in itertools.product(
        suite["topologies"],
        suite["routing_metrics"],
        suite["seeds"],
        suite["packet_sizes"],
    ):
        cases.append({
            "topology": topology,
            "routing_metric": metric,
            "seed": seed,
            "packet_size": packet_size,
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
    return f"{index:03d}-{stem}-{metric}-s{case['seed']}-p{case['packet_size']}"


def write_metrics(rows: list[dict], out: Path) -> None:
    fields = [
        "case_id",
        "topology",
        "routing_metric",
        "seed",
        "packet_size",
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
    (output / "suite.json").write_text(
        json.dumps(suite, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    return 1 if metadata["failed_cases"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
