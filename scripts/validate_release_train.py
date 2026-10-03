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

    model = data.get("model")
    if not isinstance(model, dict) or model.get("name") != "kiwi-model-v1":
        raise SystemExit("Release manifest must describe kiwi-model-v1")
    if model.get("status") not in {"pipeline_ready", "candidate", "released"}:
        raise SystemExit("Invalid KiWi Model v1 status")
    if model.get("software_component") != "kiwi":
        raise SystemExit("KiWi Model v1 must belong to the kiwi software component")
    if model.get("base_model") != "Qwen/Qwen3-1.7B":
        raise SystemExit("Unexpected KiWi Model v1 base model")
    if model["status"] == "released":
        if not model.get("promoted_artifact") or not model.get("promotion_manifest"):
            raise SystemExit("Released model requires artifact and promotion manifest")
    elif model.get("promoted_artifact") is not None or model.get("promotion_manifest") is not None:
        raise SystemExit("Unreleased model must not claim promoted artifacts")

    print(f"Validated {data['release']} ({data['status']}) with model state {model['status']}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
