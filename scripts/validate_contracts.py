from __future__ import annotations

import json
from pathlib import Path

try:
    from jsonschema import Draft202012Validator
except ImportError as exc:
    raise SystemExit(
        "Install contract validation dependencies: "
        "python -m pip install -r scripts/requirements-contracts.txt"
    ) from exc


ROOT = Path(__file__).resolve().parents[1]
CONTRACTS = ROOT / "contracts"
FIXTURES = CONTRACTS / "fixtures"


def main() -> int:
    schemas = sorted(CONTRACTS.glob("*.schema.json"))
    if not schemas:
        raise SystemExit("No contract schemas found")

    ids: set[str] = set()
    fixture_count = 0
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
        validator = Draft202012Validator(schema)
        for outcome in ("valid", "invalid"):
            fixture = FIXTURES / f"{path.name.removesuffix('.schema.json')}.{outcome}.json"
            if not fixture.is_file():
                raise SystemExit(f"Missing {outcome} fixture: {fixture}")
            payload = json.loads(fixture.read_text(encoding="utf-8"))
            errors = list(validator.iter_errors(payload))
            if outcome == "valid" and errors:
                raise SystemExit(f"{fixture}: {errors[0].message}")
            if outcome == "invalid" and not errors:
                raise SystemExit(f"{fixture}: expected schema rejection")
            fixture_count += 1

    print(f"Validated {len(schemas)} ecosystem contract schemas and {fixture_count} fixtures.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
