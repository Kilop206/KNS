#!/usr/bin/env python3
"""
KNS Runner v1.2
---------------
- Runs the simulator in headless mode (KNS --headless --topology ... --output ...)
- Generates results in ../results/v1.2/test_#N/
- Works on Windows, Linux, and macOS (cross-platform)
- Generates graphs and summaries from the aggregate stats CSV exported by the engine
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import platform
import signal
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import TextIO



# ==============================================================
# Platform utilities
# ==============================================================

def is_windows() -> bool:
    return platform.system().lower().startswith("win")


# ==============================================================
# Executable search
# ==============================================================

def find_executable(root: Path) -> Path | None:
    candidates = ["KNS.exe", "KNS"]
    for name in candidates:
        for p in root.rglob(name):
            if p.is_file() and (is_windows() or os.access(p, os.X_OK)):
                return p.resolve()
    return None


# ==============================================================
# Results directory management
# ==============================================================

def get_test_dir(base: Path) -> Path:
    base.mkdir(parents=True, exist_ok=True)
    i = 1
    while True:
        d = base / f"test_{i}"
        if not d.exists():
            d.mkdir(parents=True)
            return d
        i += 1


# ==============================================================
# Simulation headless execution
# ==============================================================

def build_command(exe: Path, topo: Path, csv_out: Path) -> list[str]:
    return [
        str(exe),
        "--headless",
        "--topology",
        str(topo),
        "--output",
        str(csv_out),
    ]


def run_silent(
    exe: Path,
    topo: Path,
    log_file: Path,
    csv_out: Path,
) -> tuple[subprocess.Popen, float, TextIO]:
    cmd = build_command(exe, topo, csv_out)

    log_handle = open(log_file, "w", encoding="utf-8")

    kwargs: dict = dict(
        stdout=log_handle,
        stderr=subprocess.STDOUT,
        stdin=subprocess.DEVNULL,
        cwd=str(topo.parent),
    )

    if is_windows():
        kwargs["creationflags"] = (
            subprocess.CREATE_NO_WINDOW | subprocess.DETACHED_PROCESS
        )
    else:
        kwargs["start_new_session"] = True

    started = time.perf_counter()
    try:
        proc = subprocess.Popen(cmd, **kwargs)
    except BaseException:
        log_handle.close()
        raise
    return proc, started, log_handle


# ==============================================================
# Stats CSV parsing
# ==============================================================

def parse_csv_if_exists(csv_file: Path) -> list[dict] | None:
    if not csv_file or not csv_file.exists():
        return None

    rows = []
    with open(csv_file, encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if None in row or any(v is None for v in row.values()):
                raise ValueError("Malformed stats CSV row")
            rows.append({k: v.strip() for k, v in row.items()})

    return rows if rows else None


def parse_stats(csv_file: Path) -> dict:
    """Read version 1 of the engine's aggregate CSV; reject invalid output."""
    rows = parse_csv_if_exists(csv_file)
    if not rows or len(rows) != 1:
        raise ValueError(f"Expected one stats row in {csv_file}")

    raw = rows[0]
    try:
        if int(raw["schema_version"]) != 1:
            raise ValueError("Unsupported schema_version")
        result = {key: int(raw[key]) for key in (
            "packets_sent", "packets_delivered", "packets_lost",
            "packets_in_transit", "total_sessions", "data_packets_delivered", "seed")}
        result.update({key: float(raw[key]) for key in (
            "total_latency", "avg_latency", "simulation_duration_s")})
        if any(not math.isfinite(value) or value < 0 for value in result.values()):
            raise ValueError("Metrics must be finite and nonnegative")
        sent = result["packets_sent"]
        result["delivery_rate"] = result["packets_delivered"] / sent if sent else 0.0
        result["loss_rate"] = result["packets_lost"] / sent if sent else 0.0
        result["avg_latency_s"] = result.pop("avg_latency")
        return result
    except (KeyError, ValueError, TypeError) as exc:
        raise ValueError(f"Invalid stats CSV {csv_file}: {exc}") from exc


# ==============================================================
# Graph generation
# ==============================================================



def _label(topo: Path) -> str:
    return topo.stem


def plot_summary_dashboard(runs: list[dict], out_dir: Path) -> Path:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.gridspec as gridspec
    import matplotlib.pyplot as plt
    import numpy as np
    COLORS = plt.rcParams["axes.prop_cycle"].by_key()["color"]
    labels = [_label(r["topo"]) for r in runs]
    n = len(labels)
    loss_rates: list[float] = []
    durations: list[float] = []
    mean_lats: list[float] = []

    for r in runs:
        stats = r["stats"]
        loss_rates.append(stats["loss_rate"] * 100 if stats else 0.0)
        durations.append(r["duration_s"])
        mean_lats.append(stats["avg_latency_s"] if stats else float("nan"))

    fig = plt.figure(figsize=(12, 8))
    gs = gridspec.GridSpec(2, 2, figure=fig, hspace=0.45, wspace=0.35)

    x = np.arange(n)
    width = 0.5

    ax1 = fig.add_subplot(gs[0, 0])
    bars = ax1.bar(x, loss_rates, width, color=[COLORS[i % len(COLORS)] for i in range(n)])
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels, rotation=20, ha="right")
    ax1.set_ylabel("Loss rate (%)")
    ax1.set_title("Loss rate per topology")
    ax1.grid(axis="y", alpha=0.3)
    for bar, val in zip(bars, loss_rates):
        ax1.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.3,
            f"{val:.1f}%",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    ax2 = fig.add_subplot(gs[0, 1])
    bars2 = ax2.bar(x, durations, width, color=[COLORS[i % len(COLORS)] for i in range(n)])
    ax2.set_xticks(x)
    ax2.set_xticklabels(labels, rotation=20, ha="right")
    ax2.set_ylabel("Duration (s)")
    ax2.set_title("Execution time per topology")
    ax2.grid(axis="y", alpha=0.3)
    for bar, val in zip(bars2, durations):
        ax2.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.1,
            f"{val:.2f}s",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    ax3 = fig.add_subplot(gs[1, 0])
    valid_indices = [i for i, v in enumerate(mean_lats) if not np.isnan(v)]
    if valid_indices:
        vals = [mean_lats[i] for i in valid_indices]
        clrs = [COLORS[i % len(COLORS)] for i in valid_indices]
        valid_labels = [labels[i] for i in valid_indices]
        ax3.bar(np.arange(len(vals)), vals, width, color=clrs)
        ax3.set_xticks(np.arange(len(vals)))
        ax3.set_xticklabels(valid_labels, rotation=20, ha="right")
        ax3.set_ylabel("Average latency (s)")
        ax3.set_title("Average latency per topology")
        ax3.grid(axis="y", alpha=0.3)
    else:
        ax3.text(
            0.5,
            0.5,
            "No latency data available",
            ha="center",
            va="center",
            transform=ax3.transAxes,
            color="gray",
        )
        ax3.set_title("Average latency per topology")

    ax4 = fig.add_subplot(gs[1, 1])
    total_lost = sum(r["stats"]["packets_lost"] for r in runs if r["stats"])
    total_deliv = sum(r["stats"]["packets_delivered"] for r in runs if r["stats"])
    if total_lost + total_deliv > 0:
        ax4.pie(
            [total_lost, total_deliv],
            labels=["Lost", "Delivered"],
            colors=["#e74c3c", "#2ecc71"],
            autopct="%1.1f%%",
            startangle=90,
        )
        ax4.set_title("Global packet distribution")
    else:
        ax4.text(
            0.5,
            0.5,
            "No stats data",
            ha="center",
            va="center",
            transform=ax4.transAxes,
            color="gray",
        )
        ax4.set_title("Global packet distribution")

    fig.suptitle("Dashboard — KNS v1.2", fontsize=14, y=0.98)

    out = out_dir / "dashboard.png"
    fig.savefig(out, dpi=130)
    plt.close(fig)
    return out


# ==============================================================
# Summaries
# ==============================================================

def compute_stats(stats: dict | None, duration_s: float) -> dict:
    result = {
        "packets_sent": stats["packets_sent"] if stats else None,
        "packets_delivered": stats["packets_delivered"] if stats else None,
        "packets_lost": stats["packets_lost"] if stats else None,
        "delivery_rate": stats["delivery_rate"] if stats else None,
        "loss_rate": stats["loss_rate"] if stats else None,
        "latency_mean_s": stats["avg_latency_s"] if stats else None,
        "seed": stats["seed"] if stats else None,
        "wall_clock_duration_s": duration_s,
        "simulation_duration_s": stats["simulation_duration_s"] if stats else None,
        "throughput_pps": (
            stats["packets_delivered"] / stats["simulation_duration_s"]
            if stats and stats["simulation_duration_s"] > 0
            else None
        ),
    }
    return result


def write_summary_json(
    runs: list[dict],
    test_dir: Path,
    run_config: dict,
    graphs: list[str],
) -> Path:
    run_results = []
    for r in runs:
        run_results.append(
            {
                "topology": str(r["topo"]),
                "log": str(r["log"]),
                "csv": str(r.get("csv")) if r.get("csv") else None,
                "returncode": r["returncode"],
                "status": r.get("status", "unknown"),
                "error": r.get("error"),
                "stats": compute_stats(r["stats"], r["duration_s"]),
            }
        )

    summary = {
        "generated_at": datetime.now().isoformat(),
        "run_config": run_config,
        "topology_count": len(runs),
        "successful_runs": sum(1 for r in runs if r["returncode"] == 0),
        "failed_runs": sum(1 for r in runs if r["returncode"] != 0),
        "graphs": graphs,
        "runs": run_results,
    }

    out = test_dir / "summary.json"
    with open(out, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    return out


def write_csv_report(runs: list[dict], test_dir: Path) -> Path:
    out = test_dir / "metrics.csv"
    fieldnames = [
        "topology",
        "wall_clock_duration_s",
        "simulation_duration_s",
        "packets_sent",
        "packets_delivered",
        "packets_lost",
        "delivery_rate_pct",
        "loss_rate_pct",
        "throughput_pps",
        "latency_mean_s",
        "seed",
        "returncode",
        "status",
    ]

    with open(out, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()

        for r in runs:
            stats = compute_stats(r["stats"], r["duration_s"])
            row = {
                "topology": r["topo"].name,
                "wall_clock_duration_s": f"{r['duration_s']:.4f}",
                "simulation_duration_s": stats["simulation_duration_s"],
                "packets_sent": stats["packets_sent"] if stats["packets_sent"] is not None else "",
                "packets_delivered": stats["packets_delivered"] if stats["packets_delivered"] is not None else "",
                "packets_lost": stats["packets_lost"] if stats["packets_lost"] is not None else "",
                "delivery_rate_pct": (
                    f"{stats['delivery_rate'] * 100:.2f}"
                    if stats["delivery_rate"] is not None
                    else ""
                ),
                "loss_rate_pct": (
                    f"{stats['loss_rate'] * 100:.2f}"
                    if stats["loss_rate"] is not None
                    else ""
                ),
                "throughput_pps": (
                    f"{stats['throughput_pps']:.2f}"
                    if stats["throughput_pps"] is not None
                    else ""
                ),
                "latency_mean_s": stats["latency_mean_s"] if stats["latency_mean_s"] is not None else "",
                "seed": stats["seed"] if stats["seed"] is not None else "",
                "returncode": r["returncode"],
                "status": r.get("status", "unknown"),
            }
            writer.writerow(row)

    return out


# ==============================================================
# Main execution
# ==============================================================

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="KNS Runner v1.2 — execute headless simulations and generate summaries"
    )
    parser.add_argument(
        "topologies_dir",
        help="Directory containing .json topology files",
    )
    parser.add_argument(
        "--max-procs",
        "-j",
        type=int,
        default=1,
        help="Maximum number of parallel simulations (default: 1)",
    )
    parser.add_argument(
        "--timeout",
        "-t",
        type=float,
        default=None,
        help="Timeout in seconds per execution (default: no limit)",
    )
    args = parser.parse_args(argv)

    if args.timeout is not None and (not math.isfinite(args.timeout) or args.timeout <= 0):
        parser.error("--timeout must be finite and positive")

    if args.max_procs < 1:
        print("[ERROR] --max-procs must be at least 1.", file=sys.stderr)
        return 1

    topo_dir = Path(args.topologies_dir).resolve()
    if not topo_dir.is_dir():
        print(f"[ERROR] Invalid directory: {topo_dir}", file=sys.stderr)
        return 1

    root = Path(__file__).resolve().parent.parent
    exe = find_executable(root)
    if not exe:
        print(
            "[ERROR] KNS(.exe) not found.\n"
            "        Build the project with CMake before running this script.",
            file=sys.stderr,
        )
        return 1

    results_root = root / "results" / "v1.2"
    test_dir = get_test_dir(results_root)
    print(f"[INFO] Results will be saved in: {test_dir}")

    topologies = sorted(topo_dir.rglob("*.json"))
    if not topologies:
        print(f"[ERROR] No .json files found in: {topo_dir}", file=sys.stderr)
        return 1

    print(f"[INFO] Found topologies: {len(topologies)}")
    print(f"[INFO] Platform: {platform.platform()}")

    pending: list[tuple[subprocess.Popen, float, Path, Path, Path, TextIO]] = []
    run_records: list[dict] = []

    for i, topo in enumerate(topologies):
        log_file = test_dir / f"run_{i}.log"
        csv_file = test_dir / f"run_{i}.csv"

        print(f"[RUN {i}] {topo.name}")
        try:
            proc, t0, log_handle = run_silent(exe, topo, log_file, csv_file)
            pending.append((proc, t0, topo, log_file, csv_file, log_handle))
        except Exception as exc:
            print(f"[ERROR] Failed to start {topo.name}: {exc}", file=sys.stderr)
            run_records.append(
                {
                    "topo": topo,
                    "log": log_file,
                    "csv": None,
                    "returncode": -1,
                    "duration_s": 0.0,
                    "stats": None,
                    "status": "startup_error",
                    "error": str(exc),
                }
            )

        while len(pending) >= args.max_procs:
            _flush_one(pending, run_records, args.timeout)

    while pending:
        _flush_one(pending, run_records, args.timeout, wait=True)

    graphs: list[str] = []

    print("[INFO] Generating graphs...")
    # Graphs are optional: a missing plotting dependency or a rendering failure
    # must not discard the diagnostics for a failed simulation batch.
    try:
        dash = plot_summary_dashboard(run_records, test_dir)
        graphs.append(str(dash))
        print("       dashboard.png")
    except Exception as exc:
        print(f"[WARNING] Dashboard unavailable: {exc}", file=sys.stderr)

    run_config = {
        "project_root": str(root),
        "topologies_dir": str(topo_dir),
        "executable": str(exe),
        "max_procs": args.max_procs,
        "timeout_seconds": args.timeout,
        "platform": platform.platform(),
    }

    with open(test_dir / "run_config.json", "w", encoding="utf-8") as f:
        json.dump(run_config, f, indent=2, ensure_ascii=False)

    summary_path = write_summary_json(run_records, test_dir, run_config, graphs)
    csv_path = write_csv_report(run_records, test_dir)

    ok = sum(1 for r in run_records if r["returncode"] == 0)
    err = len(run_records) - ok
    print()
    print("=" * 50)
    print(
        f"  Simulations completed: {ok}/{len(run_records)}"
        + (f"  ({err} with errors)" if err else "")
    )
    print(f"  Results:   {test_dir}")
    print(f"  Metrics:    {csv_path.name}")
    print(f"  Report:     {summary_path.name}")
    print(f"  Graphs:     {len(graphs)} file(s)")
    print("=" * 50)

    return 1 if err else 0


def _flush_one(
    pending: list[tuple[subprocess.Popen, float, Path, Path, Path, TextIO]],
    run_records: list[dict],
    timeout: float | None,
    wait: bool = False,
) -> None:
    """Poll every child against its launch deadline, without head-of-line blocking."""
    if not pending:
        return

    index = None
    timed_out = False
    for i, item in enumerate(pending):
        proc, t0, *_ = item
        if proc.poll() is not None:
            index = i
            break
        if timeout is not None and time.perf_counter() - t0 >= timeout:
            index = i
            timed_out = True
            _terminate_tree(proc)
            break
    if index is None:
        remaining = [max(0, t0 + timeout - time.perf_counter())
                     for _, t0, *_ in pending] if timeout is not None else [0.05]
        time.sleep(min(0.05, min(remaining)))
        return

    proc, t0, topo, log_file, csv_file, log_handle = pending[index]

    duration = time.perf_counter() - t0
    rc = proc.returncode if proc.returncode is not None else -1

    if not log_handle.closed:
        log_handle.close()

    stats = None
    error = None
    outcome = "timeout" if timed_out else ("success" if rc == 0 else "process_error")
    if timed_out:
        rc = 124
    elif rc == 0:
        try:
            stats = parse_stats(csv_file)
        except (ValueError, OSError) as exc:
            outcome, rc, error = "stats_error", 1, str(exc)

    status = "OK" if rc == 0 else f"ERROR (code {rc})"
    lost = stats["packets_lost"] if stats else "?"
    delivered = stats["packets_delivered"] if stats else "?"
    print(
        f"       -> {topo.name}: {status}  |  {duration:.2f}s  "
        f"|  {lost} lost  |  {delivered} delivered"
    )

    run_records.append(
        {
            "topo": topo,
            "log": log_file,
            "csv": csv_file if csv_file.exists() else None,
            "returncode": rc,
            "duration_s": duration,
            "stats": stats,
            "status": outcome,
            "error": error,
        }
    )
    pending.pop(index)


def _terminate_tree(proc: subprocess.Popen) -> None:
    if is_windows():
        result = subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        if result.returncode != 0 and proc.poll() is None:
            raise RuntimeError(f"Could not terminate process tree {proc.pid}")
    else:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    proc.wait()


if __name__ == "__main__":
    raise SystemExit(main())
