from __future__ import annotations

import argparse
import json
from pathlib import Path

try:
    from jsonschema import Draft202012Validator, FormatChecker
    from referencing import Registry, Resource
except ImportError as exc:
    raise SystemExit(
        "Install contract validation dependencies: "
        "python -m pip install -r scripts/requirements-contracts.txt"
    ) from exc


ROOT = Path(__file__).resolve().parents[1]
CONTRACTS = ROOT / "contracts"
FIXTURES = CONTRACTS / "fixtures"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate ecosystem schemas and fixtures")
    parser.add_argument(
        "--experiment-analysis",
        type=Path,
        action="append",
        default=[],
        help="Also validate an emitted saved-experiment analysis.json (repeatable)",
    )
    args = parser.parse_args(argv)
    schemas = sorted(CONTRACTS.glob("*.schema.json"))
    if not schemas:
        raise SystemExit("No contract schemas found")

    ids: set[str] = set()
    documents: dict[Path, dict] = {}
    for path in schemas:
        with path.open("r", encoding="utf-8") as handle:
            schema = json.load(handle)

        for required in ("$schema", "$id", "title", "type"):
            if required not in schema:
                raise SystemExit(f"{path}: missing {required}")

        if schema["$schema"] != "https://json-schema.org/draft/2020-12/schema":
            raise SystemExit(f"{path}: unsupported JSON Schema dialect")

        schema_id = schema["$id"]
        if schema_id in ids:
            raise SystemExit(f"{path}: duplicate $id {schema_id}")
        ids.add(schema_id)

        if schema["type"] != "object":
            raise SystemExit(f"{path}: top-level contract must be an object")

        Draft202012Validator.check_schema(schema)
        documents[path] = schema

    registry = Registry().with_resources(
        (schema["$id"], Resource.from_contents(schema)) for schema in documents.values()
    )
    fixture_count = 0
    for path, schema in documents.items():
        validator = Draft202012Validator(
            schema, registry=registry, format_checker=FormatChecker()
        )
        stem = path.name.removesuffix(".schema.json")
        for outcome in ("valid", "invalid"):
            fixtures = sorted(FIXTURES.glob(f"{stem}.{outcome}*.json"))
            if not fixtures:
                raise SystemExit(f"Missing {outcome} fixture for {path}")
            for fixture in fixtures:
                payload = json.loads(fixture.read_text(encoding="utf-8"))
                errors = list(validator.iter_errors(payload))
                if outcome == "valid" and errors:
                    raise SystemExit(f"{fixture}: {errors[0].message}")
                if outcome == "invalid" and not errors:
                    raise SystemExit(f"{fixture}: expected schema rejection")
                fixture_count += 1

    topology_schema = json.loads(
        (CONTRACTS / "topology-v1.schema.json").read_text(encoding="utf-8")
    )
    topology_validator = Draft202012Validator(
        topology_schema, registry=registry, format_checker=FormatChecker()
    )
    examples = sorted((ROOT / "app" / "topologies").glob("*.json"))
    for path in examples:
        payload = json.loads(path.read_text(encoding="utf-8"))
        errors = list(topology_validator.iter_errors(payload))
        if errors:
            raise SystemExit(f"{path}: {errors[0].message}")

    analysis_validator = Draft202012Validator(
        documents[CONTRACTS / "experiment-analysis-v1.schema.json"],
        registry=registry,
        format_checker=FormatChecker(),
    )
    for path in args.experiment_analysis:
        payload = json.loads(path.read_text(encoding="utf-8"))
        errors = list(analysis_validator.iter_errors(payload))
        if errors:
            raise SystemExit(f"{path}: {errors[0].message}")

    print(
        f"Validated {len(schemas)} ecosystem contract schemas, "
        f"{fixture_count} fixtures and {len(examples)} bundled topologies."
    )
    if args.experiment_analysis:
        print(f"Validated {len(args.experiment_analysis)} emitted experiment analyses.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
