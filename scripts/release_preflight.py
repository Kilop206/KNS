#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import hashlib
import re
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_E2E = {"free_plan_denied", "discovery_hub_sync", "private_topology_access",
                "optimistic_conflict", "kns_load_and_simulate", "kns_facts_sentient_kiwi",
                "quota_idempotency", "kiwi_outage", "revision_restore", "revoked_token_denied"}


def verify_evidence(path: Path | None, manifest: dict, root: Path) -> tuple[bool, list[str]]:
    if path is None:
        return False, ["Private ecosystem execution evidence is required for release readiness."]
    failures = []
    try:
        evidence = json.loads(path.read_text(encoding="utf-8"))
        if evidence.get("schema_version") != "1" or evidence.get("passed") is not True:
            failures.append("Execution evidence did not pass.")
        age = time.time() - evidence["completed_at"]
        if not 0 <= age <= 48 * 3600:
            failures.append("Execution evidence is future-dated or older than 48 hours.")
        required = set(manifest["components"])
        components = evidence["components"]
        if set(components) != required:
            failures.append("Evidence must identify every component.")
        head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
        if components.get("kns", {}).get("commit") != head:
            failures.append("Evidence does not match this KNS commit.")
        for name in required:
            component = components.get(name, {})
            if (component.get("version") != manifest["components"][name]
                    or component.get("tests_passed") is not True or component.get("clean") is not True
                    or not re.fullmatch(r"[0-9a-f]{40}", component.get("commit", ""))):
                failures.append(f"Unverified component: {name}")
        artifacts = evidence["artifacts"]
        if not required.issubset({item["component"] for item in artifacts}):
            failures.append("Missing component artifact hashes.")
        base = path.parent.resolve()
        for artifact in artifacts:
            target = (base / artifact["path"]).resolve()
            if not target.is_relative_to(base) or not target.is_file():
                failures.append("Artifact missing or outside evidence directory.")
            elif hashlib.sha256(target.read_bytes()).hexdigest() != artifact["sha256"]:
                failures.append(f"Artifact checksum mismatch: {artifact['component']}")
        for key in ("e2e", "restore_test"):
            item = evidence[key]
            target = (base / item["path"]).resolve()
            if not target.is_relative_to(base) or not target.is_file():
                failures.append(f"Missing {key} evidence.")
            elif hashlib.sha256(target.read_bytes()).hexdigest() != item["sha256"]:
                failures.append(f"Changed {key} evidence.")
            else:
                result = json.loads(target.read_text(encoding="utf-8"))
                if result.get("passed") is not True:
                    failures.append(f"Failed {key} evidence.")
                if key == "e2e" and not REQUIRED_E2E.issubset(set(result.get("checks", []))):
                    failures.append("Incomplete integration coverage.")
                if key == "restore_test" and (not result.get("tables") or
                        not re.fullmatch(r"[0-9a-f]{64}", result.get("backup_sha256", ""))):
                    failures.append("Incomplete restoration evidence.")
    except (OSError, ValueError, KeyError, TypeError, AttributeError, subprocess.SubprocessError):
        failures.append("Invalid or unreadable execution evidence.")
    return not failures, failures


def evaluate_release(root: Path = ROOT, evidence_path: Path | None = None) -> dict:
    manifest_path = root / "releases" / "ecosystem-v1.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    required_files = [
        "contracts/topology-v1.schema.json",
        "contracts/desktop-intelligence-request-v1.schema.json",
        "contracts/chat-request-v1.schema.json",
        "contracts/chat-response-v1.schema.json",
        ".github/workflows/release.yml",
        ".github/workflows/routing-benchmark-v1.yml",
        ".github/workflows/tcp-congestion-benchmark-v1.yml",
        ".github/workflows/resilience-benchmark-v1.yml",
        "benchmarks/v1/routing-baseline.json",
        "benchmarks/v1/tcp-congestion-baseline.json",
        "benchmarks/v1/resilience-baseline.json",
    ]
    missing = [path for path in required_files if not (root / path).is_file()]

    checks = {
        "manifest_status_valid": manifest.get("status") in {"candidate", "ready", "released"},
        "components_declared": set(manifest.get("components", {})) == {
            "kns", "kns_discovery", "topology_hub", "sentient_kns", "kiwi"
        },
        "contracts_declared": set(manifest.get("contracts", {})) == {
            "topology", "experiment_csv", "desktop_intelligence", "chat_request", "chat_response"
        },
        "required_files_present": not missing,
    }

    model = manifest.get("model", {})
    model_status = model.get("status")
    model_ready = (
        model_status == "released"
        and bool(model.get("promoted_artifact"))
        and bool(model.get("promotion_manifest"))
    )

    structural_ready = all(checks.values())
    evidence_ready, evidence_failures = verify_evidence(evidence_path, manifest, root)
    software_ready = structural_ready and evidence_ready
    blockers = []
    if missing:
        blockers.append("missing required release files: " + ", ".join(missing))
    for name, passed in checks.items():
        if not passed and name != "required_files_present":
            blockers.append(name)
    blockers.extend(evidence_failures)
    checks["private_execution_evidence_verified"] = evidence_ready

    return {
        "schema_version": "1.0",
        "release": manifest.get("release"),
        "manifest_status": manifest.get("status"),
        "software_ready": software_ready,
        "structural_ready": structural_ready,
        "model_ready": model_ready,
        "model_status": model_status,
        "checks": checks,
        "blockers": blockers,
        "notes": (
            []
            if model_ready
            else ["KiWi software can release independently; kiwi-model-v1 is not yet a promoted model artifact."]
        ),
    }


def markdown(report: dict) -> str:
    lines = [
        f"# Release preflight — {report['release']}",
        "",
        f"- Software ready: **{'yes' if report['software_ready'] else 'no'}**",
        f"- Structural checks: **{'pass' if report['structural_ready'] else 'fail'}**",
        f"- KiWi Model v1 ready: **{'yes' if report['model_ready'] else 'no'}**",
        f"- Manifest status: `{report['manifest_status']}`",
        f"- Model status: `{report['model_status']}`",
        "",
        "## Checks",
        "",
    ]
    for name, passed in report["checks"].items():
        lines.append(f"- {'PASS' if passed else 'FAIL'} {name}")
    if report["blockers"]:
        lines += ["", "## Blockers", ""]
        lines += [f"- {item}" for item in report["blockers"]]
    if report["notes"]:
        lines += ["", "## Notes", ""]
        lines += [f"- {item}" for item in report["notes"]]
    return "\n".join(lines) + "\n"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Validate coordinated KNS ecosystem release readiness")
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    parser.add_argument("--evidence", type=Path, help="Verified execution bundle from the private coordinator")
    parser.add_argument("--structural-only", action="store_true", help="Validate public structure without declaring release readiness")
    args = parser.parse_args(argv)

    report = evaluate_release(evidence_path=args.evidence)
    if args.json_output:
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.markdown_output:
        args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_output.write_text(markdown(report), encoding="utf-8")

    print(markdown(report), end="")
    return 0 if report["structural_ready" if args.structural_only else "software_ready"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
