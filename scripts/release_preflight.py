#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def evaluate_release(root: Path = ROOT) -> dict:
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

    software_ready = all(checks.values())
    blockers = []
    if missing:
        blockers.append("missing required release files: " + ", ".join(missing))
    for name, passed in checks.items():
        if not passed and name != "required_files_present":
            blockers.append(name)

    return {
        "schema_version": "1.0",
        "release": manifest.get("release"),
        "manifest_status": manifest.get("status"),
        "software_ready": software_ready,
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
        f"- KiWi Model v1 ready: **{'yes' if report['model_ready'] else 'no'}**",
        f"- Manifest status: `{report['manifest_status']}`",
        f"- Model status: `{report['model_status']}`",
        "",
        "## Checks",
        "",
    ]
    for name, passed in report["checks"].items():
        lines.append(f"- {'✅' if passed else '❌'} {name}")
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
    args = parser.parse_args(argv)

    report = evaluate_release()
    if args.json_output:
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.markdown_output:
        args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_output.write_text(markdown(report), encoding="utf-8")

    print(markdown(report), end="")
    return 0 if report["software_ready"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
