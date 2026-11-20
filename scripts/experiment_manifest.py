"""Portable, self-contained experiment inputs and execution provenance (stdlib only)."""

from __future__ import annotations

import copy
import hashlib
import json
import math
import platform
import re
import uuid
from datetime import datetime, timezone
from pathlib import Path, PureWindowsPath

CASE_ARTIFACT_FIELDS = ("csv", "log", "traffic_report", "analytics_report", "fault_report")


def read_json(path: Path, limit: int) -> tuple[object, str]:
    """Read and identify bounded JSON without silently discarding duplicate fields."""
    with path.open("rb") as source:
        data = source.read(limit + 1)
    if len(data) > limit:
        raise ValueError(f"{path.name} exceeds the input size limit")

    def reject(value):
        raise ValueError("Non-finite JSON numbers are unsupported: " + value)

    def finite_float(value):
        number = float(value)
        if not math.isfinite(number):
            reject(value)
        return number

    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate JSON fields are unsupported")
            result[key] = value
        return result

    try:
        value = json.loads(data.decode("utf-8"), parse_constant=reject,
                           parse_float=finite_float, object_pairs_hook=unique_object)
    except RecursionError as exc:
        raise ValueError("JSON nesting exceeds the parser limit") from exc
    return value, hashlib.sha256(data).hexdigest()


def config_hash(configuration: dict) -> str:
    encoded = json.dumps(
        configuration,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def case_artifact(directory: Path, recorded_path: str) -> Path:
    """Resolve recorded case evidence in a moved or cross-platform bundle."""
    if not isinstance(recorded_path, str) or not recorded_path:
        raise ValueError("Case artifact requires a recorded path")
    filename = PureWindowsPath(recorded_path).name
    target = (directory / "runs" / filename).resolve()
    if not filename or not target.is_relative_to((directory / "runs").resolve()):
        raise ValueError("Case artifact must stay within the bundle's runs directory")
    return target


def validate_case_ids(cases: list[dict]) -> None:
    names = [case.get("case_id") for case in cases]
    if any(
        not isinstance(name, str)
        or re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", name) is None
        for name in names
    ):
        raise ValueError("Case IDs must be ASCII identifiers without path components")
    if len({name.casefold() for name in names}) != len(names):
        raise ValueError("Case IDs must be unique, including on case-insensitive filesystems")


def verify_case_artifacts(directory: Path, cases: list[dict]) -> dict:
    coverage = {"hashed_cases": 0, "legacy_cases": 0, "verified_artifacts": 0}
    for case in cases:
        if "artifact_sha256" not in case:
            coverage["legacy_cases"] += 1
            continue
        hashes = case["artifact_sha256"]
        expected = {
            field for field in CASE_ARTIFACT_FIELDS if case.get(field) is not None
        }
        if not isinstance(hashes, dict) or not hashes or set(hashes) != expected:
            raise ValueError(
                f'Incomplete artifact SHA-256 map for case {case["case_id"]}'
            )
        for field, digest in hashes.items():
            if not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None:
                raise ValueError(f'Invalid artifact SHA-256 for case {case["case_id"]}')
            path = case_artifact(directory, case[field])
            if file_hash(path) != digest:
                raise ValueError(
                    f'Artifact SHA-256 mismatch for case {case["case_id"]} ({field})'
                )
            coverage["verified_artifacts"] += 1
        coverage["hashed_cases"] += 1
    return coverage


def source_revision(root: Path) -> str:
    """Read local provenance without invoking processes or requiring Git ownership changes."""
    try:
        git = root / ".git"
        if git.is_file():
            git = (
                root / git.read_text(encoding="utf-8").strip().removeprefix("gitdir: ")
            ).resolve()
        head = (git / "HEAD").read_text(encoding="utf-8").strip()
        if not head.startswith("ref: "):
            return head if re.fullmatch(r"[0-9a-f]{40,64}", head) else "unknown"
        ref = head[5:]
        if not ref.startswith("refs/") or ".." in Path(ref).parts:
            return "unknown"
        loose = git / ref
        if loose.is_file():
            value = loose.read_text(encoding="utf-8").strip()
            return value if re.fullmatch(r"[0-9a-f]{40,64}", value) else "unknown"
        for line in (git / "packed-refs").read_text(encoding="utf-8").splitlines():
            fields = line.split()
            if (
                len(fields) == 2
                and fields[1] == ref
                and re.fullmatch(r"[0-9a-f]{40,64}", fields[0])
            ):
                return fields[0]
    except OSError:
        pass
    return "unknown"


def source_version(root: Path) -> str:
    """Source metadata is optional in packaged or reorganized runner layouts.

    The executable hash remains the identity of the binary actually executed;
    never infer a binary version from stale build caches or unrelated files.
    """
    try:
        cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    except (OSError, UnicodeError):
        return "unknown"
    version = re.search(r"project\(KNS VERSION ([0-9.]+)", cmake)
    return version.group(1) if version else "unknown"


def create(
    profile: dict,
    topology: dict,
    timeout: float,
    exe: Path | None,
    root: Path,
    previous: dict | None = None,
    allow_change: bool = False,
) -> dict:
    portable = copy.deepcopy(profile)
    portable["topology"] = "source-topology.json"
    configuration = {
        "profile": portable,
        "topology": copy.deepcopy(topology),
        "timeout_seconds": timeout,
    }
    binary_hash = file_hash(exe) if exe else None
    expected = previous.get("executable_sha256") if previous else None
    changed = bool(expected and binary_hash and expected != binary_hash)
    if changed and not allow_change:
        raise ValueError(
            "Replay executable SHA-256 differs; use the original binary or --allow-executable-change"
        )
    return {
        "schema_version": "1.0",
        "run_id": str(uuid.uuid4()),
        "created_at": datetime.now(timezone.utc).isoformat(),
        "status": "prepared",
        "configuration": configuration,
        "configuration_sha256": config_hash(configuration),
        "source_revision": source_revision(root),
        "source_version": source_version(root),
        "executable_sha256": binary_hash,
        "environment": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "runner_sha256": {
            name: file_hash(root / "scripts" / name)
            for name in (
                "experiment_profile.py",
                "experiment_manifest.py",
                "experiment_report.py",
                "experiment_sweep.py",
                "benchmark_suite.py",
                "run.py",
                "compare_experiments.py",
                "fault_plan.py",
                "comparison_statistics.py",
                "comparison_html.py",
                "comparison_artifacts.py",
                "analytics_config.py",
                "analytics_validation.py",
                "analytics_report.py",
                "traffic_config.py",
                "validate_result.py",
                "validation_html.py",
            )
        },
        "replay_of": previous["run_id"] if previous else None,
        "executable_changed": changed,
    }


def load(path: Path) -> dict:
    value, _ = read_json(path, 16 * 1024 * 1024)
    if not isinstance(value, dict) or value.get("schema_version") != "1.0":
        raise ValueError("Unsupported experiment manifest schema_version")
    configuration = value.get("configuration")
    if not isinstance(configuration, dict) or set(configuration) != {
        "profile",
        "topology",
        "timeout_seconds",
    }:
        raise ValueError(
            "Manifest configuration must contain profile, topology and timeout_seconds"
        )
    if not isinstance(configuration["profile"], dict) or not isinstance(
        configuration["topology"], dict
    ):
        raise ValueError("Manifest profile and topology must be objects")
    timeout = configuration["timeout_seconds"]
    if type(timeout) not in (int, float) or not math.isfinite(timeout) or timeout <= 0:
        raise ValueError("Manifest timeout_seconds must be finite and positive")
    if config_hash(configuration) != value.get("configuration_sha256"):
        raise ValueError("Manifest configuration SHA-256 mismatch")
    try:
        uuid.UUID(value.get("run_id", ""))
    except (ValueError, AttributeError, TypeError) as exc:
        raise ValueError("Manifest run_id must be a UUID") from exc
    binary_hash = value.get("executable_sha256")
    if binary_hash is not None and (
        not isinstance(binary_hash, str)
        or not re.fullmatch(r"[0-9a-f]{64}", binary_hash)
    ):
        raise ValueError(
            "Manifest executable_sha256 must be a SHA-256 hex string or null"
        )
    return value


def save(output: Path, manifest: dict) -> None:
    temporary = output / "manifest.json.tmp"
    temporary.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    temporary.replace(output / "manifest.json")


def finish(output: Path, manifest: dict, returncode: int) -> None:
    benchmark = json.loads(
        (output / "runs" / "benchmark.json").read_text(encoding="utf-8")
    )
    manifest.update(
        status="completed" if returncode == 0 else "failed",
        finished_at=datetime.now(timezone.utc).isoformat(),
        returncode=returncode,
        case_count=benchmark["case_count"],
        successful_cases=benchmark["successful_cases"],
        failed_cases=benchmark["failed_cases"],
        results="runs/benchmark.json",
    )
    save(output, manifest)
