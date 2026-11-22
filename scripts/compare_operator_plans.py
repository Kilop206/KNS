"""Compare validated Network Operator plans offline; never execute their steps."""

from __future__ import annotations

import argparse
import html
import json
import sys
from pathlib import Path

from experiment_manifest import read_json

ROOT = Path(__file__).resolve().parents[1]
MAX_PLAN_BYTES = 4 * 1024 * 1024


class ComparisonParser(argparse.ArgumentParser):
    def error(self, message):
        self.print_usage(sys.stderr)
        self.exit(1, f"{self.prog}: error: {message}\n")


def pointer(path: tuple) -> str:
    return "".join("/" + str(part).replace("~", "~0").replace("/", "~1") for part in path)


def compare(reference: object, candidate: object, max_changes: int = 200) -> dict:
    """Compare JSON trees, preserving array order, missing/null and bool/number distinctions."""
    if type(max_changes) is not int or not 1 <= max_changes <= 1000:
        raise ValueError("max-changes must be between 1 and 1000")
    missing = object()
    pending = [((), reference, candidate)]
    changes = []
    count = 0
    while pending:
        path, before, after = pending.pop()
        if isinstance(before, dict) and isinstance(after, dict):
            for key in sorted(before.keys() | after.keys(), reverse=True):
                pending.append(((*path, key), before.get(key, missing), after.get(key, missing)))
            continue
        if isinstance(before, list) and isinstance(after, list):
            for index in reversed(range(max(len(before), len(after)))):
                pending.append(((*path, index), before[index] if index < len(before) else missing,
                                after[index] if index < len(after) else missing))
            continue
        numeric = type(before) in (int, float) and type(after) in (int, float)
        if (numeric or type(before) is type(after)) and before == after:
            continue
        count += 1
        if len(changes) < max_changes:
            change = {"kind": "added" if before is missing else "removed" if after is missing else "changed",
                      "path": pointer(path)}
            if before is not missing:
                change["before"] = before
            if after is not missing:
                change["after"] = after
            changes.append(change)
    return {"schema_version": "1.0", "status": "different" if count else "equivalent",
            "change_count": count, "reported_changes": len(changes),
            "truncated": count > len(changes), "changes": changes}


def load_plan(path: Path, label: str, validator) -> tuple[object, str]:
    try:
        if not path.is_file():
            raise ValueError("not a regular file")
        plan, digest = read_json(path, MAX_PLAN_BYTES)
    except (OSError, ValueError) as exc:
        # JSON errors can include input values; keep CLI diagnostics independent of plan contents.
        raise ValueError(f"{label}: cannot read a strict UTF-8 JSON plan (maximum 4 MiB)") from exc
    pending = [(plan, 0)]
    while pending:
        value, depth = pending.pop()
        if isinstance(value, (dict, list)):
            if depth >= 64:
                raise ValueError(f"{label}: JSON nesting exceeds 64 levels")
            children = value.values() if isinstance(value, dict) else value
            pending.extend((child, depth + 1) for child in children)
    error = next(validator.iter_errors(plan), None)
    if error is not None:
        # Escape non-ASCII/control characters in pointers, without including the rejected value.
        location = json.dumps(pointer(tuple(error.absolute_path)), ensure_ascii=True)
        raise ValueError(f"{label}: invalid operator-plan-v1 at JSON pointer {location}")
    return plan, digest


def render_html(report: dict) -> str:
    def escaped(value):
        return html.escape(json.dumps(value, ensure_ascii=True, sort_keys=True))

    rows = []
    for change in report["changes"]:
        before = "<em>absent</em>" if "before" not in change else escaped(change["before"])
        after = "<em>absent</em>" if "after" not in change else escaped(change["after"])
        rows.append(f'<tr><td>{escaped(change["path"])}</td><td>{escaped(change["kind"])}</td>'
                    f'<td><pre>{before}</pre></td><td><pre>{after}</pre></td></tr>')
    return ('<!doctype html><html lang="en"><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width, initial-scale=1">'
            '<meta http-equiv="Content-Security-Policy" content="default-src \'none\'; style-src \'unsafe-inline\'">'
            '<title>Operator plan comparison</title><style>'
            'body{font:16px system-ui;margin:2rem;max-width:100rem}table{border-collapse:collapse;width:100%}'
            'th,td{border:1px solid #aaa;padding:.6rem;text-align:left;vertical-align:top}'
            'pre,code{white-space:pre-wrap;overflow-wrap:anywhere} .table{overflow-x:auto}'
            '</style><h1>Operator plan comparison</h1><p>Offline review only. No steps were executed.</p>'
            f'<p>Status: <strong>{escaped(report["status"])}</strong>. '
            f'Differences: {report["change_count"]}; shown: {report["reported_changes"]}; '
            f'truncated: {str(report["truncated"]).lower()}.</p>'
            f'<p>Reference SHA-256: <code>{escaped(report["reference_sha256"])}</code><br>'
            f'Candidate SHA-256: <code>{escaped(report["candidate_sha256"])}</code></p>'
            '<div class="table"><table><thead><tr><th>JSON pointer</th><th>Change</th><th>Before</th><th>After</th>'
            '</tr></thead><tbody>' + ''.join(rows) + '</tbody></table></div></html>')


def main(argv: list[str] | None = None) -> int:
    parser = ComparisonParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--output", type=Path, required=True, help="New report directory (must not exist)")
    parser.add_argument("--max-changes", type=int, default=200)
    args = parser.parse_args(argv)
    try:
        if not 1 <= args.max_changes <= 1000:
            raise ValueError("max-changes must be between 1 and 1000")
        if args.output.exists():
            raise ValueError("output directory already exists; choose a new path")
        try:
            from jsonschema import Draft202012Validator
        except ImportError as exc:
            raise ValueError("Install dependencies: python -m pip install -r scripts/requirements-contracts.txt") from exc
        schema = json.loads((ROOT / "contracts/operator-plan-v1.schema.json").read_text(encoding="utf-8"))
        validator = Draft202012Validator(schema)
        reference, reference_hash = load_plan(args.reference, "reference", validator)
        candidate, candidate_hash = load_plan(args.candidate, "candidate", validator)
        report = compare(reference, candidate, args.max_changes)
        report.update(reference_sha256=reference_hash, candidate_sha256=candidate_hash)
        encoded = json.dumps(report, indent=2, ensure_ascii=True, allow_nan=False) + "\n"
        page = render_html(report)
        args.output.mkdir(parents=True, exist_ok=False)
        (args.output / "comparison.json").write_text(encoded, encoding="utf-8")
        (args.output / "report.html").write_text(page, encoding="utf-8")
    except (OSError, ValueError, RecursionError) as exc:
        # Input/validation errors above contain no plan values; OS errors only name local files.
        message = "JSON nesting exceeds the supported limit" if isinstance(exc, RecursionError) else str(exc)
        print(f"Comparison failed: {message}", file=sys.stderr)
        return 1
    print(f'{report["status"]}: {report["change_count"]} differences; report: {args.output / "report.html"}')
    return 2 if report["change_count"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
