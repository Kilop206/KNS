"""Validate reproducible result artifacts written by the real KNS batch runner."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import uuid
from pathlib import Path

from jsonschema import Draft202012Validator, FormatChecker
from referencing import Registry, Resource
from validate_analysis_context import ROOT, find_executable


def validate(work_dir: Path) -> None:
    executable = find_executable()
    work = work_dir / f"simulation-contract-{uuid.uuid4().hex}"
    inputs = work / "inputs"
    output = work / "output"
    topology = (ROOT / "app/topologies/mesh4.json").read_bytes()
    for name in ("first", "second"):
        source = inputs / name / "mesh4.json"
        source.parent.mkdir(parents=True)
        source.write_bytes(topology)

    result = subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts/run.py"),
            str(inputs),
            "--exe",
            str(executable),
            "--output-root",
            str(output),
            "--max-procs",
            "2",
            "--timeout",
            "30",
            "--seed",
            str(2**64 - 1),
            "--packet-size",
            "512",
        ],
        env={**os.environ, "KNS_AUTO_START": "1"},
        capture_output=True,
        text=True,
        timeout=75,
    )
    if result.returncode:
        raise SystemExit(
            f"KNS batch failed ({result.returncode}):\n{result.stdout}\n{result.stderr}"
        )

    contracts = ROOT / "contracts"
    config_schema = json.loads(
        (contracts / "simulation.config.schema.json").read_text(encoding="utf-8")
    )
    result_schema = json.loads(
        (contracts / "simulation.result.schema.json").read_text(encoding="utf-8")
    )
    registry = Registry().with_resource(
        config_schema["$id"], Resource.from_contents(config_schema)
    )
    validator = Draft202012Validator(
        result_schema, registry=registry, format_checker=FormatChecker()
    )
    batch = output / "test_1"
    summary = json.loads((batch / "summary.json").read_text(encoding="utf-8"))
    if summary["successful_runs"] != 2:
        raise SystemExit("The batch did not report both runs as successful")

    results = []
    engine_sha256 = hashlib.sha256(executable.read_bytes()).hexdigest()
    for run in summary["runs"]:
        path = Path(run["result_json"])
        payload = json.loads(path.read_text(encoding="utf-8"))
        validator.validate(payload)
        config = payload["config"]
        snapshot = (path.parent / config["topology_snapshot"]).read_bytes()
        if (
            snapshot != topology
            or hashlib.sha256(snapshot).hexdigest() != config["topology_sha256"]
        ):
            raise SystemExit("The recorded topology does not match the simulated input")
        if (
            config["engine_sha256"] != engine_sha256
            or config["seed"] != "ffffffffffffffff"
        ):
            raise SystemExit("Engine hash or uint64 seed did not round-trip")
        if payload["metrics"]["data_packets_delivered"] <= 0:
            raise SystemExit("The example simulation did not deliver traffic")
        if not (path.parent / "results/ai_context.json").is_file():
            raise SystemExit("The run did not retain its own analysis context")
        results.append(payload)

    first, second = results
    if first["config"] != second["config"] or first["metrics"] != second["metrics"]:
        raise SystemExit(
            "Identical configurations produced different simulation metrics"
        )
    if first["run_id"] == second["run_id"]:
        raise SystemExit("Independent runs shared an identifier")
    print(
        "Validated two independent simulation results with identical inputs and metrics"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--work-dir",
        type=Path,
        required=True,
        help="Parent for contract test artifacts",
    )
    validate(parser.parse_args().work_dir.resolve())
