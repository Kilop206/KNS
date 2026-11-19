#!/usr/bin/env python3
"""Rebuild comparison, validation and overview reports from a saved experiment."""

from __future__ import annotations

import argparse
import copy
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

import benchmark_suite
import compare_experiments
import experiment_manifest as manifests
import experiment_profile
import experiment_report
import validate_result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Completed experiment bundle")
    parser.add_argument("--output", type=Path, required=True, help="New report directory")
    args = parser.parse_args(argv)
    output = args.output.resolve()
    created = False
    try:
        source = args.directory.resolve()
        manifest = manifests.load(source / "manifest.json")
        if manifest.get("status") not in {"completed", "failed"}:
            raise ValueError("Analysis requires a finished experiment bundle")
        config = manifest["configuration"]
        profile, _ = experiment_profile.validate_profile(
            copy.deepcopy(config["profile"]), config["topology"]
        )
        benchmark, benchmark_hash = validate_result.read_json(
            source / "runs/benchmark.json", 64 * 1024 * 1024
        )
        comparison = compare_experiments.build_comparison(profile, benchmark)
        manifests.validate_case_ids(benchmark["cases"])
        expected_count = len(benchmark_suite.expand_cases(benchmark["suite"]))
        if (
            len(benchmark["cases"]) != expected_count
            or benchmark.get("case_count") != expected_count
        ):
            raise ValueError("Benchmark must contain every case in the profile matrix")
        accepted = {"success"}
        if profile["allow_transport_failures"]:
            accepted.add("transport_failure")
        result = int(any(
            case["status"] not in accepted
            or type(case.get("returncode")) is not int
            or case["returncode"] != (0 if case["status"] == "success" else 2)
            for case in benchmark["cases"]
        ))
        output.mkdir(parents=True, exist_ok=False)
        created = True
        validation = None
        validator_hash = manifests.file_hash(Path(validate_result.__file__))
        if "validation" in profile:
            validation = experiment_profile.validate_cases(
                source, profile, validator_hash, output
            )
            if validation["error_cases"]:
                result = 1
            elif result == 0 and validation["failed_cases"]:
                result = 2
        compare_experiments.write_reports(comparison, output)
        analysis = {
            "schema_version": "1.0",
            "status": "passed" if result == 0 else "failed",
            "returncode": result,
            "analyzed_at": datetime.now(timezone.utc).isoformat(),
            "source_run_id": manifest["run_id"],
            "source_manifest_sha256": manifests.file_hash(source / "manifest.json"),
            "source_benchmark_sha256": benchmark_hash,
            "configuration_sha256": manifest["configuration_sha256"],
            "source_execution_status": manifest["status"],
            "source_execution_returncode": manifest.get("returncode"),
            "analyzer_sha256": manifests.file_hash(Path(__file__)),
            "comparison_sha256": manifests.file_hash(Path(compare_experiments.__file__)),
            "validator_sha256": validator_hash,
            "renderer_sha256": manifests.file_hash(Path(experiment_report.__file__)),
            "report": "report.html",
            "validation": "validation/summary.json" if validation else None,
        }
        experiment_profile.write_json(output / "analysis.json", analysis)
        experiment_report.write(output, manifest, comparison, validation, source)
        print(json.dumps({"status": analysis["status"], "report": str(output / "report.html")}))
        return result
    except (OSError, ValueError, KeyError, TypeError, OverflowError, RecursionError) as exc:
        if created:
            experiment_profile.write_json(output / "analysis.json", {
                "schema_version": "1.0", "status": "error", "error": str(exc), "returncode": 1,
            })
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
