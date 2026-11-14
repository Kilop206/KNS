#!/usr/bin/env python3
"""Evaluate bounded sensor growth/link alternatives using actual saved KNS runs."""
import argparse
import copy
import csv
from decimal import Decimal, ROUND_CEILING
import hashlib
import json
from pathlib import Path
import re
import sys

import experiment_profile
import industrial_scenario
import traffic_config


def costs_from(path, bandwidths):
    if path is None:
        return None
    with path.open("rb") as source:
        raw = source.read(256 * 1024 + 1)
    if len(raw) > 256 * 1024:
        raise ValueError("Cost input exceeds 256 KiB")
    data = json.loads(raw)
    if not isinstance(data, dict) or not {"schema_version", "currency", "uplinks"} <= set(data) or set(data) - {"schema_version", "currency", "uplinks", "sensor_unit_cost"}:
        raise ValueError("Costs require schema_version, currency and uplinks; optional sensor_unit_cost")
    if data["schema_version"] != "1.0" or not isinstance(data["currency"], str) or not re.fullmatch(r"[A-Za-z0-9]{1,16}", data["currency"]):
        raise ValueError("Invalid cost version/currency label")
    if not isinstance(data["uplinks"], list) or not 1 <= len(data["uplinks"]) <= 20:
        raise ValueError("Costs require 1..20 uplink alternatives")
    seen = set()
    for uplink in data["uplinks"]:
        if not isinstance(uplink, dict) or set(uplink) != {"bandwidth_mbps", "cost"}:
            raise ValueError("Uplink cost requires bandwidth_mbps and cost")
        bandwidth = traffic_config.number(uplink["bandwidth_mbps"], 1e-9, 1e9, "bandwidth_mbps")
        traffic_config.number(uplink["cost"], 0, 1e15, "cost")
        if bandwidth in seen:
            raise ValueError("Duplicate bandwidth cost")
        seen.add(bandwidth)
    if set(bandwidths) != seen:
        raise ValueError("User costs must cover exactly the evaluated bandwidth alternatives")
    if "sensor_unit_cost" in data:
        traffic_config.number(data["sensor_unit_cost"], 0, 1e15, "sensor_unit_cost")
    return data


def populations(base, growth):
    traffic_config.integer(base, 1, 250, "base sensors")
    if not 1 <= len(growth) <= 20 or len(set(growth)) != len(growth):
        raise ValueError("Growth percentages require 1..20 distinct values")
    result = []
    for percent in growth:
        traffic_config.number(percent, 0, 10000, "growth percentage")
        count = int((Decimal(base) * (1 + Decimal(str(percent)) / 100)).to_integral_value(rounding=ROUND_CEILING))
        traffic_config.integer(count, 1, 250, "grown sensor count")
        if count in result:
            raise ValueError("Different growth percentages select the same sensor count")
        result.append(count)
    return result


def pareto(rows):
    """Cost/latency minimized; measured delivery/timely delivery maximized.

    Each population is compared separately. Equal alternatives remain coequal.
    No extrapolation, automatic winner or cross-population dominance is inferred.
    """
    if any(row["user_cost"] is None for row in rows):
        for row in rows:
            row["pareto_efficient"] = None
        return
    for row in rows:
        def dominates(other):
            if other["sensor_count"] != row["sensor_count"]:
                return False
            pairs = ((other["user_cost"], row["user_cost"]),
                     (other["mean_network_data_latency_s"], row["mean_network_data_latency_s"]),
                     (-other["delivery_ratio"], -row["delivery_ratio"]),
                     (-other["sensor_on_time_ratio"], -row["sensor_on_time_ratio"]))
            return all(a <= b for a, b in pairs) and any(a < b for a, b in pairs)
        row["pareto_efficient"] = not any(dominates(other) for other in rows)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="New directory; every alternative is actually simulated")
    parser.add_argument("--base-sensors", type=int, default=100)
    parser.add_argument("--growth-percent", nargs="+", type=float, default=[0, 10, 25, 50, 100])
    parser.add_argument("--bandwidth-mbps", nargs="+", type=float, default=[2, 4])
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--max-cases", type=int, default=32)
    parser.add_argument("--min-delivery", type=float, default=0.99)
    parser.add_argument("--min-sensor-on-time", type=float, default=0.99)
    parser.add_argument("--redundant", action="store_true")
    parser.add_argument("--with-outage", action="store_true", help="Apply the same primary-uplink outage to every alternative")
    parser.add_argument("--costs", type=Path, help="Optional user-declared cost JSON; no prices are assumed")
    args = parser.parse_args(argv)
    try:
        counts = populations(args.base_sensors, args.growth_percent)
        bandwidths = args.bandwidth_mbps
        if not 1 <= len(bandwidths) <= 20 or len(set(bandwidths)) != len(bandwidths):
            raise ValueError("Bandwidths require 1..20 distinct alternatives")
        for bandwidth in bandwidths:
            traffic_config.number(bandwidth, 1e-9, 1e9, "bandwidth_mbps")
        traffic_config.integer(args.seed, 0, 2**64 - 1, "seed")
        traffic_config.integer(args.max_cases, 1, 100, "max_cases")
        if len(counts) * len(bandwidths) > args.max_cases:
            raise ValueError("Capacity matrix exceeds max_cases")
        for threshold in (args.min_delivery, args.min_sensor_on_time):
            traffic_config.number(threshold, 0, 1, "requirement ratio")
        if not args.exe.is_file():
            raise ValueError("KNS executable does not exist")
        costs = costs_from(args.costs, bandwidths)
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=False)
        config = {"schema_version": "1.0", "base_sensors": args.base_sensors, "growth_percent": args.growth_percent,
            "sensor_counts": counts, "bandwidth_mbps": bandwidths, "seed": args.seed, "max_cases": args.max_cases,
            "requirements": {"min_delivery": args.min_delivery, "min_sensor_on_time": args.min_sensor_on_time},
            "redundant": args.redundant, "with_outage": args.with_outage, "user_costs": costs}
        industrial_scenario.save(output / "configuration.json", config)
        rows = []
        for population_index, count in enumerate(counts):
            directory = output / f"population-{count}"; directory.mkdir()
            topology, profile = industrial_scenario.configuration(count, args.redundant, args.seed)
            outage = profile["experiments"][1]["fault_plan"]
            profile.update(name=f"capacity-{count}", topology=str(directory / "topology.json"), max_cases=len(bandwidths),
                experiments=[{"name": f"alternative-{index:02d}", "bandwidth": bandwidth,
                    **({"fault_plan": copy.deepcopy(outage)} if args.with_outage else {})} for index, bandwidth in enumerate(bandwidths)])
            industrial_scenario.save(directory / "topology.json", topology)
            industrial_scenario.save(directory / "profile.json", profile)
            code = experiment_profile.main(["--profile", str(directory / "profile.json"), "--exe", str(args.exe.resolve()), "--output", str(directory / "experiment")])
            if code:
                raise ValueError(f"Population {count} has failed/incomplete runs; inspect retained experiment evidence")
            benchmark = json.loads((directory / "experiment/runs/benchmark.json").read_text())
            for bandwidth, case in zip(bandwidths, benchmark["cases"]):
                report = json.loads(Path(case["traffic_report"]).read_text())
                traffic_config.validate_report(report, report["config"])
                sensor_flows = [f for f in report["flows"] if f["id"] <= count]
                timely = sum(f["delivered_on_time"] for f in sensor_flows) / sum(f["generated"] for f in sensor_flows)
                delivery = report["delivered"] / report["generated"]
                cost = None
                if costs:
                    uplink = next(item for item in costs["uplinks"] if item["bandwidth_mbps"] == bandwidth)
                    cost = float(Decimal(str(uplink["cost"])) + count * Decimal(str(costs.get("sensor_unit_cost", 0))))
                rows.append({"sensor_count": count, "growth_percent": args.growth_percent[population_index],
                    "bandwidth_mbps": bandwidth, "generated": report["generated"], "delivered": report["delivered"],
                    "dropped": report["dropped"], "delivery_ratio": delivery, "sensor_on_time_ratio": timely,
                    "mean_network_data_latency_s": case["avg_latency_s"], "user_cost": cost,
                    "meets_requirements": delivery >= args.min_delivery and timely >= args.min_sensor_on_time,
                    "manifest": str(directory / "experiment/manifest.json"), "traffic_report": case["traffic_report"],
                    "wall_clock_duration_s": case["wall_clock_duration_s"]})
        pareto(rows)
        limits = [{"bandwidth_mbps": bandwidth, "highest_observed_feasible_sensor_count":
            max((r["sensor_count"] for r in rows if r["bandwidth_mbps"] == bandwidth and r["meets_requirements"]), default=None)} for bandwidth in bandwidths]
        result = {"schema_version": "1.0", "configuration": config, "case_count": len(rows), "results": rows,
            "observed_feasible_populations": limits, "frontier_status": "user_cost_frontier" if costs else "no_user_costs",
            "planner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "limitations": "Finite sampled populations under the declared one-second workload/deadlines; no extrapolated capacity, monotonicity assumption, confidence interval, price estimate or global winner. Costs cover user-declared uplinks and optional sensors only; not TCO."}
        industrial_scenario.save(output / "capacity.json", result)
        with (output / "capacity.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
        cost_label = "User cost (" + costs["currency"] + ")" if costs else "User cost unavailable"
        lines = ["# Observed capacity alternatives", "", f"| Sensors | Mbps | Delivery | Sensor on-time | Meets requirements | {cost_label} | Pareto |", "| ---: | ---: | ---: | ---: | --- | ---: | --- |"]
        for row in rows:
            lines.append(f"| {row['sensor_count']} | {row['bandwidth_mbps']} | {row['delivery_ratio']:.6f} | {row['sensor_on_time_ratio']:.6f} | {row['meets_requirements']} | {row['user_cost']} | {row['pareto_efficient']} |")
        (output / "capacity.md").write_text("\n".join(lines) + "\n\n" + result["limitations"] + "\n", encoding="utf-8")
        print("Capacity evidence: " + str(output / "capacity.json"))
    except (OSError, ValueError, KeyError, TypeError, OverflowError) as error:
        print("[ERROR] " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
