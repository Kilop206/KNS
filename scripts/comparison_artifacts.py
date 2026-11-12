"""Compare selected variants in independent saved bundles, without running KNS."""
import copy
import json
from pathlib import Path

import experiment_manifest


def read(directory: Path) -> tuple[dict, dict]:
    profile = json.loads((directory / "profile.json").read_text(encoding="utf-8"))
    benchmark = json.loads((directory / "runs/benchmark.json").read_text(encoding="utf-8"))
    return profile, benchmark


def build(reference: Path, candidate: Path, reference_name=None, candidate_name=None,
          reference_routing=None, candidate_routing=None) -> dict:
    from compare_experiments import build_comparison, DEFAULTS
    left, left_runs = read(reference)
    right, right_runs = read(candidate)
    reference_name = reference_name or left.get("baseline", left["experiments"][0]["name"])
    candidate_name = candidate_name or right.get("baseline", right["experiments"][0]["name"])
    build_comparison(left, left_runs, reference_name)
    build_comparison(right, right_runs, candidate_name)
    if bool(reference_routing) != bool(candidate_routing):
        raise ValueError("Specify both --reference-routing and --candidate-routing")
    profiles, runs = [left, right], [left_runs, right_runs]
    names = [reference_name, candidate_name]
    routes = [reference_routing, candidate_routing]
    paths = ["001-reference.json", "002-candidate.json"]
    sources = {}
    dimensions = {key: [] for key in DEFAULTS}
    rows = []
    fault_plans = {}
    for side, (directory, profile, benchmark, name, route) in enumerate(zip(
            (reference, candidate), profiles, runs, names, routes)):
        variant_index = next(i for i, item in enumerate(profile["experiments"]) if item["name"] == name)
        source_path = benchmark["suite"]["topologies"][variant_index]
        if route and route not in profile.get("routing_metrics", DEFAULTS["routing_metrics"]):
            raise ValueError("Selected routing metric is absent from its artifact bundle")
        for key, defaults in DEFAULTS.items():
            values = profile.get(key, defaults)
            if key == "routing_metrics" and reference_routing:
                values = [reference_routing]  # explicit declared route mapping
            dimensions[key] = list(dict.fromkeys([*dimensions[key], *values]))
        mapped_plan = benchmark["suite"].get("fault_plans", {}).get(source_path)
        if mapped_plan is not None:
            fault_plans[paths[side]] = mapped_plan
        for original in benchmark["cases"]:
            if original["topology"] != source_path or route and original["routing_metric"] != route:
                continue
            row = copy.deepcopy(original)
            row["topology"] = paths[side]
            if reference_routing:
                row["routing_metric"] = reference_routing
            rows.append(row)
        source = {"directory": str(directory), "experiment": name,
                  "configuration": profile["experiments"][variant_index],
                  "kns_commit": benchmark.get("kns_commit"), "selected_routing_metric": route}
        if (directory / "manifest.json").is_file():
            manifest = experiment_manifest.load(directory / "manifest.json")
            source.update(configuration_sha256=manifest["configuration_sha256"],
                          executable_sha256=manifest.get("executable_sha256"), environment=manifest.get("environment"))
        sources["reference" if side == 0 else "candidate"] = source
    profile = {"name": "cross-artifact-comparison", "experiments": [{"name": "reference"}, {"name": "candidate"}], **dimensions}
    suite = {"schema_version": "1.0", "name": profile["name"], "topologies": paths, **dimensions,
             "link_event_scenarios": [{"name": "baseline", "events": []}], "fault_plans": fault_plans}
    result = build_comparison(profile, {"schema_version": "1.0", "suite": suite, "cases": rows}, "reference")
    if reference_routing:
        for row in [*result["pairs"], *result["summaries"]]:
            row["baseline_routing_metric"] = reference_routing
            row["experiment_routing_metric"] = candidate_routing
            row["routing_metric"] = f"{reference_routing} vs {candidate_routing}"
        result["methodology"]["pairing"] = ["explicit_routing_pair", "seed", "packet_size", "congestion_control"]
    result["sources"] = sources
    result["methodology"]["limitations"].append(
        "Across bundles, matching seed/payload/algorithm settings does not establish equal offered traffic, endpoint roles, observation horizons or software environments. Inspect recorded inputs; causality is not inferred.")
    if sources["reference"].get("executable_sha256") != sources["candidate"].get("executable_sha256"):
        result["methodology"]["limitations"].append("Executable identities differ or are incomplete; software changes can confound the comparison.")
    return result
