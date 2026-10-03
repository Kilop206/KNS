from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTRACTS = ROOT / "contracts"


def main() -> int:
    schemas = sorted(CONTRACTS.glob("*.schema.json"))
    if not schemas:
        raise SystemExit("No contract schemas found")

    ids: set[str] = set()
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

    print(f"Validated {len(schemas)} ecosystem contract schemas.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
