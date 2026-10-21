#!/usr/bin/env python3
"""Generate controlled link variants and run them with the benchmark runner."""
from __future__ import annotations

import argparse
import copy
import json
import math
from datetime import datetime, timezone
from pathlib import Path
import re
import sys

import benchmark_suite
import compare_experiments
import run as runner


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PROFILE = ROOT / "KNS Experiments" / "experiment-profile.json"
PARAMETERS = {"bandwidth", "delay", "loss"}


def valid_name(value: object) -> bool:
    return isinstance(value, str) and re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", value) is not None


def load_profile(path: Path) -> tuple[dict, dict]:
    profile = json.loads(path.read_text(encoding="utf-8"))
    allowed = {"name", "topology", "index", "experiments", "seeds", "packet_sizes",
               "routing_metrics", "congestion_controls", "allow_transport_failures", "baseline"}
    if not isinstance(profile, dict) or set(profile) - allowed:
        raise ValueError("Profile must be an object containing only supported fields")
    if not valid_name(profile.get("name")):
        raise ValueError("Profile name must use letters, digits, hyphens or underscores")
    if not isinstance(profile.get("topology"), str) or not profile["topology"]:
        raise ValueError("Profile topology must be a non-empty path relative to the KNS root")
    topology = json.loads((ROOT / profile["topology"]).read_text(encoding="utf-8"))
    if not isinstance(topology, dict) or not isinstance(topology.get("links"), list):
        raise ValueError("Topology must contain a links array")
    index = profile.setdefault("index", 0)
    if type(index) is not int or not 0 <= index < len(topology["links"]):
        raise ValueError("Profile index must select an existing link")
    link = topology["links"][index]
    if not isinstance(link, dict):
        raise ValueError("Selected link must be an object")
    experiments = profile.get("experiments")
    if not isinstance(experiments, list) or not experiments:
        raise ValueError("Profile experiments must be a non-empty list")
    names = set()
    for experiment in experiments:
        if not isinstance(experiment, dict) or set(experiment) - ({"name"} | PARAMETERS):
            raise ValueError("Each experiment must contain name and supported link parameters")
        name = experiment.get("name")
        if not valid_name(name) or name.casefold() in names:
            raise ValueError("Experiment names must be valid and unique (case-insensitive)")
        names.add(name.casefold())
        changes = set(experiment) & PARAMETERS
        if not changes:
            raise ValueError(f"Experiment {name} must modify at least one link parameter")
        for field in changes:
            value = experiment[field]
            if field not in link:
                raise ValueError(f"Selected link has no {field} field")
            if type(value) not in (int, float) or not math.isfinite(value):
                raise ValueError(f"{field} must be a finite number")
            if (field == "bandwidth" and value <= 0
                    or field == "delay" and value < 0
                    or field == "loss" and not 0 <= value <= 1):
                raise ValueError(f"Invalid {field}: bandwidth > 0, delay >= 0, loss in [0, 1]")

    dimensions = {
        "seeds": [42], "packet_sizes": [1500],
        "routing_metrics": ["delay"], "congestion_controls": ["reno"],
    }
    for field, default in dimensions.items():
        values = profile.setdefault(field, default)
        if not isinstance(values, list) or not values:
            raise ValueError(f"{field} must be a non-empty list")
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
                raise ValueError(f"Invalid {field} value: {value!r}")
        if len(set(values)) != len(values):
            raise ValueError(f"{field} must not contain duplicates; use distinct seeds for repetitions")
    if type(profile.setdefault("allow_transport_failures", False)) is not bool:
        raise ValueError("allow_transport_failures must be a boolean")
    baseline = profile.setdefault("baseline", experiments[0]["name"])
    if not isinstance(baseline, str) or baseline not in {experiment["name"] for experiment in experiments}:
        raise ValueError("baseline must name an experiment in the profile")
    return profile, topology


def make_suite(profile: dict, output: Path) -> dict:
    return {
        "schema_version": "1.0",
        "name": profile["name"],
        "topologies": [str(output / "topologies" / f"{index:03d}-{experiment['name']}.json")
                       for index, experiment in enumerate(profile["experiments"], start=1)],
        **{key: profile[key] for key in ("seeds", "packet_sizes", "routing_metrics",
                                        "congestion_controls", "allow_transport_failures")},
        "link_event_scenarios": [{"name": "baseline", "events": []}],
    }


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + "\n",
                    encoding="utf-8")


def generate(profile: dict, topology: dict, output: Path) -> Path:
    # Refuse existing directories so no prior run or source file is overwritten.
    output.mkdir(parents=True, exist_ok=False)
    (output / "topologies").mkdir()
    suite = make_suite(profile, output)
    for experiment, filename in zip(profile["experiments"], suite["topologies"]):
        variant = copy.deepcopy(topology)
        variant["links"][profile["index"]].update(
            {key: experiment[key] for key in PARAMETERS if key in experiment})
        write_json(Path(filename), variant)
    write_json(output / "profile.json", profile)
    write_json(output / "source-topology.json", topology)
    suite_path = output / "suite.json"
    write_json(suite_path, suite)
    return suite_path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    parser.add_argument("--output", type=Path, help="New artifact directory (must not exist)")
    parser.add_argument("--exe", type=Path, help="Built KNS executable")
    parser.add_argument("--timeout", type=float, default=60.0, help="Seconds per case")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="Validate and print cases without writing files")
    mode.add_argument("--generate-only", action="store_true", help="Write variants and suite without running KNS")
    args = parser.parse_args(argv)
    try:
        if not math.isfinite(args.timeout) or args.timeout <= 0:
            raise ValueError("--timeout must be finite and positive")
        profile, topology = load_profile(args.profile.resolve())
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        output = (args.output or ROOT / "KNS Experiments" / "results" / profile["name"] / timestamp).resolve()
        suite = make_suite(profile, output)
        if args.dry_run:
            cases = benchmark_suite.expand_cases(suite)
            print(json.dumps({"profile": profile["name"], "baseline": profile["baseline"], "case_count": len(cases),
                              "cases": cases}, indent=2))
            return 0
        exe = None
        if not args.generate_only:
            exe = args.exe.resolve() if args.exe else runner.find_executable(ROOT / "build")
            if not exe or not exe.is_file():
                raise ValueError("KNS executable not found; build KNS or pass --exe")
        suite_path = generate(profile, topology, output)
        print(f"Experiment artifacts: {output}")
        if args.generate_only:
            return 0
        result = benchmark_suite.main(["--suite", str(suite_path), "--exe", str(exe),
                                       "--output", str(output / "runs"), "--timeout", str(args.timeout)])
        comparison = compare_experiments.compare_artifacts(output)
        print(f"Comparison: {output / 'comparison.md'} "
              f"({comparison['matched_pairs']}/{comparison['expected_pairs']} matched pairs)")
        return result
    except (OSError, ValueError, OverflowError) as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
