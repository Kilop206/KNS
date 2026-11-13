"""Portable, self-contained experiment inputs and execution provenance (stdlib only)."""
from __future__ import annotations

import copy
from datetime import datetime, timezone
import hashlib
import json
import math
import platform
from pathlib import Path
import re
import uuid


def config_hash(configuration: dict) -> str:
    encoded = json.dumps(configuration, sort_keys=True, separators=(",", ":"),
                         ensure_ascii=False, allow_nan=False).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_revision(root: Path) -> str:
    """Read local provenance without invoking processes or requiring Git ownership changes."""
    try:
        git = root / ".git"
        if git.is_file():
            git = (root / git.read_text(encoding="utf-8").strip().removeprefix("gitdir: ")).resolve()
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
            if len(fields) == 2 and fields[1] == ref and re.fullmatch(r"[0-9a-f]{40,64}", fields[0]):
                return fields[0]
    except OSError:
        pass
    return "unknown"


def create(profile: dict, topology: dict, timeout: float, exe: Path | None,
           root: Path, previous: dict | None = None, allow_change: bool = False) -> dict:
    portable = copy.deepcopy(profile)
    portable["topology"] = "source-topology.json"
    configuration = {"profile": portable, "topology": copy.deepcopy(topology),
                     "timeout_seconds": timeout}
    binary_hash = file_hash(exe) if exe else None
    expected = previous.get("executable_sha256") if previous else None
    changed = bool(expected and binary_hash and expected != binary_hash)
    if changed and not allow_change:
        raise ValueError("Replay executable SHA-256 differs; use the original binary or --allow-executable-change")
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    version = re.search(r"project\(KNS VERSION ([0-9.]+)", cmake)
    return {
        "schema_version": "1.0", "run_id": str(uuid.uuid4()),
        "created_at": datetime.now(timezone.utc).isoformat(), "status": "prepared",
        "configuration": configuration, "configuration_sha256": config_hash(configuration),
        "source_revision": source_revision(root),
        "source_version": version.group(1) if version else "unknown",
        "executable_sha256": binary_hash,
        "environment": {"platform": platform.platform(), "machine": platform.machine(),
                        "python": platform.python_version()},
        "runner_sha256": {name: file_hash(root / "scripts" / name) for name in (
            "experiment_profile.py", "experiment_manifest.py", "experiment_sweep.py",
            "benchmark_suite.py", "run.py", "compare_experiments.py", "fault_plan.py",
            "comparison_statistics.py", "comparison_html.py", "comparison_artifacts.py", "analytics_config.py",
            "analytics_validation.py", "analytics_report.py")},
        "replay_of": previous["run_id"] if previous else None,
        "executable_changed": changed,
    }


def load(path: Path) -> dict:
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError("Manifest exceeds 16 MiB")
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or value.get("schema_version") != "1.0":
        raise ValueError("Unsupported experiment manifest schema_version")
    configuration = value.get("configuration")
    if not isinstance(configuration, dict) or set(configuration) != {"profile", "topology", "timeout_seconds"}:
        raise ValueError("Manifest configuration must contain profile, topology and timeout_seconds")
    if not isinstance(configuration["profile"], dict) or not isinstance(configuration["topology"], dict):
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
    if binary_hash is not None and (not isinstance(binary_hash, str) or not re.fullmatch(r"[0-9a-f]{64}", binary_hash)):
        raise ValueError("Manifest executable_sha256 must be a SHA-256 hex string or null")
    return value


def save(output: Path, manifest: dict) -> None:
    temporary = output / "manifest.json.tmp"
    temporary.write_text(json.dumps(manifest, indent=2, ensure_ascii=False, allow_nan=False) + "\n",
                         encoding="utf-8")
    temporary.replace(output / "manifest.json")


def finish(output: Path, manifest: dict, returncode: int) -> None:
    benchmark = json.loads((output / "runs" / "benchmark.json").read_text(encoding="utf-8"))
    manifest.update(status="completed" if returncode == 0 else "failed",
                    finished_at=datetime.now(timezone.utc).isoformat(), returncode=returncode,
                    case_count=benchmark["case_count"],
                    successful_cases=benchmark["successful_cases"], failed_cases=benchmark["failed_cases"],
                    results="runs/benchmark.json")
    save(output, manifest)
