"""Validate the real KNS headless analysis context against its wire schema."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import uuid

from jsonschema import Draft202012Validator


ROOT = Path(__file__).resolve().parents[1]


def find_executable() -> Path:
    for relative in (
        "build/app/KNS.exe",
        "build/app/Release/KNS.exe",
        "build/app/KNS",
        "build/app/Release/KNS",
    ):
        candidate = ROOT / relative
        if candidate.is_file():
            return candidate
    raise SystemExit("Build the KNS application before validating its emitted context")


def validate(work_dir: Path) -> None:
    run_dir = work_dir / f"context-{uuid.uuid4().hex}"
    run_dir.mkdir(parents=True)
    command = [
        str(find_executable()),
        "--headless",
        "--topology",
        str(ROOT / "app" / "topologies" / "mesh4.json"),
        "--output",
        str(run_dir / "simulation.csv"),
    ]
    result = subprocess.run(command, cwd=run_dir, capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise SystemExit(f"KNS headless failed ({result.returncode}): {result.stderr.strip()}")

    output = run_dir / "results" / "ai_context.json"
    context = json.loads(output.read_text(encoding="utf-8"))
    schema = json.loads((ROOT / "contracts" / "analysis.context.schema.json").read_text(encoding="utf-8"))
    Draft202012Validator(schema).validate(context)
    print(f"Validated emitted KNS {context['depth']} analysis context")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", type=Path, help="Parent for generated test artifacts")
    options = parser.parse_args()
    if options.work_dir is not None:
        validate(options.work_dir.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="kns-context-") as temporary:
            validate(Path(temporary))


if __name__ == "__main__":
    main()
