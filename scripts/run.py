#!/usr/bin/env python3
"""
KNS Runner v0.9
---------------
- Generates results in ../results/v0.9/test_#N/
- Works on Windows, Linux, and macOS (cross-platform)
- Generates graphs and summaries about latency, time, and other system data
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import TextIO

import matplotlib

matplotlib.use("Agg")
import matplotlib.gridspec as gridspec
import matplotlib.pyplot as plt
import numpy as np


# ==============================================================
# Platform utilities
# ==============================================================

def is_windows() -> bool:
    return platform.system().lower().startswith("win")


def is_linux() -> bool:
    return platform.system().lower() == "linux"


def has_display() -> bool:
    if is_windows():
        return True
    return bool(os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"))


def xvfb_available() -> bool:
    try:
        subprocess.run(["xvfb-run", "--help"], capture_output=True, timeout=3, check=False)
        return True
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False


# ==============================================================
# Executable search
# ==============================================================

def find_executable(root: Path) -> Path | None:
    candidates = ["kns_app.exe", "kns_app"]
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
# Simulation silent execution
# ==============================================================

def build_command(exe: Path, topo: Path, use_xvfb: bool) -> list[str]:
    cmd = [str(exe), str(topo)]
    if use_xvfb:
        cmd = ["xvfb-run", "--auto-servernum", "--"] + cmd
    return cmd


def run_silent(
    exe: Path,
    topo: Path,
    log_file: Path,
    timeout: float | None = None,
) -> tuple[subprocess.Popen, float, TextIO]:
    use_xvfb = is_linux() and not has_display() and xvfb_available()
    cmd = build_command(exe, topo, use_xvfb)

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

    proc = subprocess.Popen(cmd, **kwargs)
    return proc, time.perf_counter(), log_handle


# ==============================================================
# System output log parsing
# ==============================================================

_RE_DROPPED = re.compile(
    r"\[DROPPED\]\s+Packet\s+from\s+(\d+)\s+to\s+(\d+)\s+at\s+time\s+([\d.]+)"
)
_RE_DELIVERED = re.compile(
    r"\[DELIVERED\]\s+Packet\s+from\s+(\d+)\s+to\s+(\d+)\s+.*?latency[=:\s]+([\d.]+)",
    re.IGNORECASE,
)
_RE_LATENCY = re.compile(
    r"\[LATENCY\]\s+([\d.]+)",
    re.IGNORECASE,
)


def parse_log(log_file: Path) -> dict:
    drops: list[tuple[int, int, float]] = []
    deliveries: list[tuple[int, int, float]] = []
    latencies: list[float] = []

    if not log_file.exists():
        return {"drops": drops, "deliveries": deliveries, "latencies": latencies}

    with open(log_file, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = _RE_DROPPED.search(line)
            if m:
                drops.append((int(m.group(1)), int(m.group(2)), float(m.group(3))))
                continue
            m = _RE_DELIVERED.search(line)
            if m:
                deliveries.append((int(m.group(1)), int(m.group(2)), float(m.group(3))))
                latencies.append(float(m.group(3)))
                continue
            m = _RE_LATENCY.search(line)
            if m:
                latencies.append(float(m.group(1)))

    return {"drops": drops, "deliveries": deliveries, "latencies": latencies}


def parse_csv_if_exists(csv_file: Path) -> dict | None:
    if not csv_file or not csv_file.exists():
        return None

    rows = []
    with open(csv_file, encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({k: v.strip() for k, v in row.items() if k is not None})

    return rows if rows else None


# ==============================================================
# Graph generation
# ==============================================================

COLORS = plt.rcParams["axes.prop_cycle"].by_key()["color"]


def _label(topo: Path) -> str:
    return topo.stem


def plot_drops_over_time(runs: list[dict], out_dir: Path) -> Path | None:
    fig, ax = plt.subplots(figsize=(10, 5))
    any_data = False

    for i, run in enumerate(runs):
        events = run["events"]
        drop_times = sorted(t for _, _, t in events["drops"])
        if not drop_times:
            continue

        t_max = max(drop_times)
        bins = np.linspace(0, t_max, min(50, len(drop_times) // 2 + 2))
        counts, edges = np.histogram(drop_times, bins=bins)
        centers = (edges[:-1] + edges[1:]) / 2
        window = edges[1] - edges[0] if len(edges) > 1 else 1.0

        label = _label(run["topo"])
        ax.plot(
            centers,
            counts / window,
            label=label,
            color=COLORS[i % len(COLORS)],
        )
        any_data = True

    if not any_data:
        plt.close(fig)
        return None

    ax.set_xlabel("Simulation time (s)")
    ax.set_ylabel("Dropped packets / s")
    ax.set_title("Drop rate over time")
    ax.legend()
    ax.grid(alpha=0.3)
    fig.tight_layout()

    out = out_dir / "drops_over_time.png"
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out


def plot_latency_distribution(runs: list[dict], out_dir: Path) -> Path | None:
    fig, ax = plt.subplots(figsize=(10, 5))
    any_data = False

    for i, run in enumerate(runs):
        lats = run["events"]["latencies"]
        if not lats:
            continue
        ax.hist(
            lats,
            bins=30,
            alpha=0.65,
            label=_label(run["topo"]),
            color=COLORS[i % len(COLORS)],
            edgecolor="white",
        )
        any_data = True

    if not any_data:
        plt.close(fig)
        return None

    ax.set_xlabel("Latency (s)")
    ax.set_ylabel("Frequency")
    ax.set_title("Latency distribution")
    ax.legend()
    ax.grid(alpha=0.3)
    fig.tight_layout()

    out = out_dir / "latency_distribution.png"
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out


def plot_drop_heatmap(runs: list[dict], out_dir: Path) -> list[Path]:
    outs: list[Path] = []

    for run in runs:
        drops = run["events"]["drops"]
        if not drops:
            continue

        nodes = sorted({n for d in drops for n in d[:2]})
        n = len(nodes)
        idx = {v: i for i, v in enumerate(nodes)}
        mat = np.zeros((n, n), dtype=int)

        for src, dst, _ in drops:
            if src in idx and dst in idx:
                mat[idx[src], idx[dst]] += 1

        fig, ax = plt.subplots(figsize=(6, 5))
        im = ax.imshow(mat, cmap="Reds", aspect="auto")
        ax.set_xticks(range(n))
        ax.set_xticklabels([f"N{v}" for v in nodes])
        ax.set_yticks(range(n))
        ax.set_yticklabels([f"N{v}" for v in nodes])
        ax.set_xlabel("Destination")
        ax.set_ylabel("Source")
        ax.set_title(f"Drops per pair — {_label(run['topo'])}")
        plt.colorbar(im, ax=ax, label="Dropped packets")

        for r in range(n):
            for c in range(n):
                if mat[r, c]:
                    ax.text(
                        c,
                        r,
                        str(mat[r, c]),
                        ha="center",
                        va="center",
                        fontsize=8,
                        color="white" if mat[r, c] > mat.max() * 0.6 else "black",
                    )

        fig.tight_layout()
        out = out_dir / f"drop_heatmap_{_label(run['topo'])}.png"
        fig.savefig(out, dpi=120)
        plt.close(fig)
        outs.append(out)

    return outs


def plot_latency_over_time(runs: list[dict], out_dir: Path) -> Path | None:
    """Latency delivered over time, when available."""
    fig, ax = plt.subplots(figsize=(10, 5))
    any_data = False

    for i, run in enumerate(runs):
        deliveries = run["events"]["deliveries"]
        if not deliveries:
            continue
        times = [t for _, _, t in deliveries]
        lats = [l for _, _, l in deliveries]
        ax.scatter(
            times,
            lats,
            s=8,
            alpha=0.5,
            label=_label(run["topo"]),
            color=COLORS[i % len(COLORS)],
        )
        any_data = True

    if not any_data:
        plt.close(fig)
        return None

    ax.set_xlabel("Arrival time (s)")
    ax.set_ylabel("Latency (s)")
    ax.set_title("Latency per packet over time")
    ax.legend()
    ax.grid(alpha=0.3)
    fig.tight_layout()

    out = out_dir / "latency_over_time.png"
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out


def plot_summary_dashboard(runs: list[dict], out_dir: Path) -> Path:
    labels = [_label(r["topo"]) for r in runs]
    n = len(labels)
    drop_rates: list[float] = []
    durations: list[float] = []
    mean_lats: list[float] = []

    for r in runs:
        drops = len(r["events"]["drops"])
        deliveries = len(r["events"]["deliveries"])
        total = drops + deliveries
        drop_rates.append(drops / total * 100 if total > 0 else 0.0)
        durations.append(r["duration_s"])
        lats = r["events"]["latencies"]
        mean_lats.append(float(np.mean(lats)) if lats else float("nan"))

    fig = plt.figure(figsize=(12, 8))
    gs = gridspec.GridSpec(2, 2, figure=fig, hspace=0.45, wspace=0.35)

    x = np.arange(n)
    width = 0.5

    ax1 = fig.add_subplot(gs[0, 0])
    bars = ax1.bar(x, drop_rates, width, color=[COLORS[i % len(COLORS)] for i in range(n)])
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels, rotation=20, ha="right")
    ax1.set_ylabel("Drop rate (%)")
    ax1.set_title("Drop rate per topology")
    ax1.grid(axis="y", alpha=0.3)
    for bar, val in zip(bars, drop_rates):
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
    total_drops = sum(len(r["events"]["drops"]) for r in runs)
    total_deliv = sum(len(r["events"]["deliveries"]) for r in runs)
    if total_drops + total_deliv > 0:
        ax4.pie(
            [total_drops, total_deliv],
            labels=["Dropped", "Delivered"],
            colors=["#e74c3c", "#2ecc71"],
            autopct="%1.1f%%",
            startangle=90,
        )
        ax4.set_title("Global packet distribution")
    else:
        ax4.text(
            0.5,
            0.5,
            "No event data",
            ha="center",
            va="center",
            transform=ax4.transAxes,
            color="gray",
        )
        ax4.set_title("Global packet distribution")

    fig.suptitle("Dashboard — KNS v0.9", fontsize=14, y=0.98)

    out = out_dir / "dashboard.png"
    fig.savefig(out, dpi=130)
    plt.close(fig)
    return out


# ==============================================================
# Summaries
# ==============================================================

def compute_stats(events: dict, duration_s: float) -> dict:
    drops = events["drops"]
    deliveries = events["deliveries"]
    lats = events["latencies"]
    total = len(drops) + len(deliveries)

    result = {
        "packets_total": total,
        "packets_dropped": len(drops),
        "packets_delivered": len(deliveries),
        "drop_rate": len(drops) / total if total else None,
        "delivery_rate": len(deliveries) / total if total else None,
        "duration_seconds": duration_s,
        "throughput_pps": len(deliveries) / duration_s if duration_s > 0 else None,
    }

    if lats:
        arr = np.array(lats)
        result.update(
            {
                "latency_mean_s": float(np.mean(arr)),
                "latency_median_s": float(np.median(arr)),
                "latency_min_s": float(np.min(arr)),
                "latency_max_s": float(np.max(arr)),
                "latency_p95_s": float(np.percentile(arr, 95)),
                "latency_std_s": float(np.std(arr)),
            }
        )
    else:
        result.update(
            {
                "latency_mean_s": None,
                "latency_median_s": None,
                "latency_min_s": None,
                "latency_max_s": None,
                "latency_p95_s": None,
                "latency_std_s": None,
            }
        )

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
                "stats": compute_stats(r["events"], r["duration_s"]),
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
        "duration_s",
        "packets_total",
        "packets_dropped",
        "packets_delivered",
        "drop_rate_pct",
        "delivery_rate_pct",
        "throughput_pps",
        "latency_mean_s",
        "latency_median_s",
        "latency_min_s",
        "latency_max_s",
        "latency_p95_s",
        "latency_std_s",
        "returncode",
    ]

    with open(out, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()

        for r in runs:
            stats = compute_stats(r["events"], r["duration_s"])
            row = {
                "topology": r["topo"].name,
                "duration_s": f"{r['duration_s']:.4f}",
                "packets_total": stats["packets_total"],
                "packets_dropped": stats["packets_dropped"],
                "packets_delivered": stats["packets_delivered"],
                "drop_rate_pct": (
                    f"{stats['drop_rate'] * 100:.2f}" if stats["drop_rate"] is not None else ""
                ),
                "delivery_rate_pct": (
                    f"{stats['delivery_rate'] * 100:.2f}"
                    if stats["delivery_rate"] is not None
                    else ""
                ),
                "throughput_pps": (
                    f"{stats['throughput_pps']:.2f}"
                    if stats["throughput_pps"] is not None
                    else ""
                ),
                "latency_mean_s": stats["latency_mean_s"] if stats["latency_mean_s"] is not None else "",
                "latency_median_s": stats["latency_median_s"] if stats["latency_median_s"] is not None else "",
                "latency_min_s": stats["latency_min_s"] if stats["latency_min_s"] is not None else "",
                "latency_max_s": stats["latency_max_s"] if stats["latency_max_s"] is not None else "",
                "latency_p95_s": stats["latency_p95_s"] if stats["latency_p95_s"] is not None else "",
                "latency_std_s": stats["latency_std_s"] if stats["latency_std_s"] is not None else "",
                "returncode": r["returncode"],
            }
            writer.writerow(row)

    return out


# ==============================================================
# Main execution
# ==============================================================

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="KNS Runner v0.9 — execute simulations and generate summaries"
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
            "[ERROR] kns_app(.exe) not found.\n"
            "        Build the project with CMake before running this script.",
            file=sys.stderr,
        )
        return 1

    results_root = root / "results" / "v0.9"
    test_dir = get_test_dir(results_root)
    print(f"[INFO] Results will be saved in: {test_dir}")

    topologies = sorted(topo_dir.rglob("*.json"))
    if not topologies:
        print(f"[ERROR] No .json files found in: {topo_dir}", file=sys.stderr)
        return 1

    print(f"[INFO] Found topologies: {len(topologies)}")
    print(f"[INFO] Platform: {platform.platform()}")
    if is_linux() and not has_display():
        if xvfb_available():
            print("[INFO] Linux without display — using xvfb-run for headless mode")
        else:
            print(
                "[WARNING] Linux without display and without xvfb-run. "
                "The execution may fail if the binary requires a window."
            )

    pending: list[tuple[subprocess.Popen, float, Path, Path, Path, TextIO]] = []
    run_records: list[dict] = []

    for i, topo in enumerate(topologies):
        log_file = test_dir / f"run_{i}.log"
        csv_file = test_dir / f"run_{i}.csv"

        print(f"[RUN {i}] {topo.name}")
        try:
            proc, t0, log_handle = run_silent(exe, topo, log_file, args.timeout)
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
                    "events": {"drops": [], "deliveries": [], "latencies": []},
                }
            )

        while len(pending) >= args.max_procs:
            _flush_one(pending, run_records, args.timeout)

    while pending:
        _flush_one(pending, run_records, args.timeout, wait=True)

    graphs: list[str] = []

    print("[INFO] Generating graphs...")
    dash = plot_summary_dashboard(run_records, test_dir)
    graphs.append(str(dash))
    print("       dashboard.png")

    out = plot_drops_over_time(run_records, test_dir)
    if out:
        graphs.append(str(out))
        print("       drops_over_time.png")

    out = plot_latency_distribution(run_records, test_dir)
    if out:
        graphs.append(str(out))
        print("       latency_distribution.png")

    out = plot_latency_over_time(run_records, test_dir)
    if out:
        graphs.append(str(out))
        print("       latency_over_time.png")

    heatmaps = plot_drop_heatmap(run_records, test_dir)
    for h in heatmaps:
        graphs.append(str(h))
        print(f"       {h.name}")

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

    return 0


def _flush_one(
    pending: list[tuple[subprocess.Popen, float, Path, Path, Path, TextIO]],
    run_records: list[dict],
    timeout: float | None,
    wait: bool = False,
) -> None:
    """Check or wait for the first pending process, then record its result."""
    if not pending:
        return

    proc, t0, topo, log_file, csv_file, log_handle = pending[0]

    if wait:
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
    else:
        try:
            proc.wait(timeout=0.05)
        except subprocess.TimeoutExpired:
            return

    duration = time.perf_counter() - t0
    rc = proc.returncode if proc.returncode is not None else -1

    if not log_handle.closed:
        log_handle.close()

    events = parse_log(log_file)

    status = "OK" if rc == 0 else f"ERROR (code {rc})"
    print(
        f"       -> {topo.name}: {status}  |  {duration:.2f}s  "
        f"|  {len(events['drops'])} drops  "
        f"|  {len(events['deliveries'])} delivered"
    )

    run_records.append(
        {
            "topo": topo,
            "log": log_file,
            "csv": csv_file if csv_file.exists() else None,
            "returncode": rc,
            "duration_s": duration,
            "events": events,
        }
    )
    pending.pop(0)


if __name__ == "__main__":
    raise SystemExit(main())