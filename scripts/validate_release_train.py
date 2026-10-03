from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "releases" / "ecosystem-v1.json"


def main() -> int:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("format_version") != "1.0":
        raise SystemExit("Unsupported release manifest format")
    if data.get("status") not in {"candidate", "ready", "released"}:
        raise SystemExit("Invalid release status")
    components = data.get("components", {})
    required = {"kns", "kns_discovery", "topology_hub", "sentient_kns", "kiwi"}
    if set(components) != required:
        raise SystemExit(f"Release components must be exactly {sorted(required)}")
    semver = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
    for name, version in components.items():
        if not isinstance(version, str) or not semver.fullmatch(version):
            raise SystemExit(f"{name}: invalid semantic version {version!r}")

    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(KNS VERSION ([^ ]+) LANGUAGES", cmake)
    if not match or match.group(1) != components["kns"]:
        raise SystemExit("KNS CMake version does not match release manifest")

    expected_contracts = {
        "topology": "1.0",
        "experiment_csv": "1",
        "desktop_intelligence": "v1",
        "chat_request": "v1",
        "chat_response": "v1",
    }
    if data.get("contracts") != expected_contracts:
        raise SystemExit("Release manifest contract set does not match ecosystem v1")

    print(f"Validated {data['release']} ({data['status']}).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
