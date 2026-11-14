#!/usr/bin/env python3
"""Analyze bounded trace metadata or convert it into an ordered abstract replay."""
import argparse
import bisect
import csv
from decimal import Decimal, InvalidOperation, localcontext
import hashlib
import io
import json
import math
from pathlib import Path
import re
import statistics
import sys

import experiment_manifest
import experiment_profile
import pcap_trace
import pcapng_trace
import traffic_config

LIMIT = 4 * 1024 * 1024
FIELDS = {"time_s", "flow_id", "source", "destination", "wire_bytes"}


def read(path):
    with path.open("rb") as source:
        data = source.read(LIMIT + 1)
    if len(data) > LIMIT:
        raise ValueError("Trace/topology input exceeds 4 MiB")
    return data


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON field")
        result[key] = value
    return result


def json_input(data, decimal=False):
    def reject(value):
        raise ValueError("Non-finite JSON constant")
    return json.loads(data.decode("utf-8-sig"), object_pairs_hook=unique_object,
                      parse_constant=reject, parse_float=Decimal if decimal else float)


def convert(data, format_name, topology, seed=42, keep_flow_labels=False, address_map=None):
    if len(data) > LIMIT:
        raise ValueError("Trace input exceeds 4 MiB")
    traffic_config.integer(seed, 0, 2**64 - 1, "seed")
    if type(keep_flow_labels) is not bool:
        raise ValueError("keep_flow_labels must be a boolean")
    if not isinstance(topology, dict) or not isinstance(topology.get("nodes"), list) or not topology["nodes"] or not isinstance(topology.get("links"), list) or not topology["links"]:
        raise ValueError("A modeled topology with nodes and links is required")
    if address_map is not None and format_name not in ("pcap", "pcapng"):
        raise ValueError("Address maps are used only for PCAP/PCAPNG import")
    pcap_metadata = None
    if format_name == "csv":
        reader = csv.DictReader(io.StringIO(data.decode("utf-8-sig")), strict=True)
        if reader.fieldnames is None or len(reader.fieldnames) != len(FIELDS) or set(reader.fieldnames) != FIELDS:
            raise ValueError("CSV header requires exactly time_s, flow_id, source, destination, wire_bytes")
        rows = []
        for row in reader:
            if len(rows) >= 10000:
                raise ValueError("Trace exceeds 10000 packets")
            if set(row) != FIELDS or any(value is None for value in row.values()):
                raise ValueError("CSV record has missing/extra columns")
            for key in ("source", "destination", "wire_bytes"):
                if not re.fullmatch(r"[0-9]+", row[key]):
                    raise ValueError("CSV endpoint/size fields must be nonnegative integers")
                row[key] = int(row[key])
            rows.append(row)
    elif format_name == "json":
        document = json_input(data, decimal=True)
        if not isinstance(document, dict) or set(document) != {"schema_version", "packets"} or document["schema_version"] != "1.0":
            raise ValueError("JSON trace requires schema_version 1.0 and packets")
        rows = document["packets"]
    elif format_name in ("pcap", "pcapng"):
        reader = pcap_trace if format_name == "pcap" else pcapng_trace
        rows, pcap_metadata = reader.parse(data, address_map, len(topology["nodes"]))
    else:
        raise ValueError("Supported trace formats are csv, json, classic pcap and the documented pcapng subset")
    if not isinstance(rows, list) or not 1 <= len(rows) <= 10000:
        raise ValueError("Trace requires 1..10000 packets")
    groups, labels, metadata_groups, events = {}, {}, [], []
    origin = previous = previous_float = None
    wire_total = collisions = 0
    for index, row in enumerate(rows, 1):
        if not isinstance(row, dict) or set(row) != FIELDS:
            raise ValueError(f"Trace packet {index} has missing/unsupported fields")
        raw_time = row["time_s"]
        allowed_time_types = (str, int) if format_name == "csv" else (int, Decimal)
        if type(raw_time) not in allowed_time_types:
            raise ValueError(f"Trace packet {index} timestamp must be numeric seconds")
        try:
            timestamp = Decimal(str(raw_time))
        except InvalidOperation as error:
            raise ValueError(f"Trace packet {index} timestamp is invalid") from error
        if not timestamp.is_finite() or timestamp.copy_abs() > Decimal("1e18") or (previous is not None and timestamp < previous):
            raise ValueError("Trace timestamps must be finite, ordered seconds with magnitude <=1e18")
        if origin is None: origin = timestamp
        # PCAPNG clocks can use 2^-127 units and signed 64-bit offsets.
        # Their exact decimal offsets fit 200 digits; avoid a 28-digit
        # intermediate rounding before the final binary64 conversion.
        with localcontext() as context:
            context.prec = 200
            relative = timestamp - origin
        at = float(relative)
        if not math.isfinite(at) or at >= 1e6:
            raise ValueError("Normalized trace duration must be below 1000000 seconds")
        if previous is not None and timestamp != previous and at == previous_float:
            collisions += 1
        previous, previous_float = timestamp, at
        source = traffic_config.integer(row["source"], 0, len(topology["nodes"]) - 1, "trace source")
        destination = traffic_config.integer(row["destination"], 0, len(topology["nodes"]) - 1, "trace destination")
        size = traffic_config.integer(row["wire_bytes"], 1, 1048576, "trace wire_bytes")
        label = row["flow_id"]
        if type(label) not in (str, int) or not 1 <= len(str(label)) <= 128 or any(ord(char) < 32 for char in str(label)):
            raise ValueError("Trace flow_id must be a short nonempty string/integer identifier")
        label = str(label)
        logical_flow = (label, source, destination)
        if logical_flow not in labels: labels[logical_flow] = len(labels) + 1
        key = (label, source, destination, size)
        if key not in groups:
            if len(groups) >= 256:
                raise ValueError("Trace exceeds 256 endpoint/flow/wire-size groups")
            identifier = len(groups) + 1
            groups[key] = identifier
            metadata_groups.append({"id": identifier, "source_flow_id": labels[logical_flow], "source": source, "destination": destination,
                "wire_bytes": size, "packet_count": 0, **({"source_flow_label": label} if keep_flow_labels else {})})
        identifier = groups[key]
        metadata_groups[identifier - 1]["packet_count"] += 1
        events.append({"at_s": at, "flow_id": identifier})
        wire_total += size
    end = max(1e-9, math.nextafter(events[-1]["at_s"], math.inf))
    flows = [{"id": group["id"], "source": group["source"], "destination": group["destination"],
              "wire_bytes": group["wire_bytes"], "start_s": 0, "end_s": end, "schedule": {"kind": "replay"}} for group in metadata_groups]
    plan = {"schema_version": "1.0", "seed": seed, "max_packets": len(events), "flows": flows, "replay_events": events}
    traffic_config.validate(plan, topology)
    intervals = [right["at_s"] - left["at_s"] for left, right in zip(events, events[1:])]
    edges = [1e-6, 1e-5, 1e-4, 0.001, 0.01, 0.1, 1]
    bins = [0] * (len(edges) + 1)
    for value in intervals: bins[bisect.bisect_right(edges, value)] += 1
    size_counts = {}
    for group in metadata_groups:
        size_counts[group["wire_bytes"]] = size_counts.get(group["wire_bytes"], 0) + group["packet_count"]
    metadata = {"schema_version": "1.0", "format": format_name, "source_sha256": hashlib.sha256(data).hexdigest(),
        "importer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "plan_sha256": experiment_manifest.config_hash(plan), "packet_count": len(events), "source_flow_count": len(labels),
        "wire_bytes_total": wire_total, "normalized_duration_s": events[-1]["at_s"], "absolute_time_origin_removed": True,
        "source_flow_labels_preserved": keep_flow_labels, "floating_time_collisions": collisions, "decimal_normalization_precision": 200, "wire_groups": metadata_groups,
        "wire_size_distribution": [{"wire_bytes": size, "packets": count} for size, count in sorted(size_counts.items())],
        "interarrival_s": {"scope": "ordered normalized binary64 source offsets", "count": len(intervals),
            "min": min(intervals) if intervals else None, "max": max(intervals) if intervals else None,
            "mean": statistics.fmean(intervals) if intervals else None, "median": statistics.median(intervals) if intervals else None,
            "sample_stddev": statistics.stdev(intervals) if len(intervals) > 1 else None,
            "histogram": {"upper_edges_s": edges, "counts": bins, "bins": "lower inclusive, upper exclusive"}},
        "limitations": "Explicit model-node mapping, abstract offered wire packets; variable sizes split into separate Analytics flow groups; no payload/protocol replay or physical topology inference; flow-label suppression does not anonymize topology metadata"}
    if pcap_metadata is not None: metadata[format_name] = pcap_metadata
    return plan, metadata


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--topology", type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--output", type=Path, help="New import bundle; source trace is never copied")
    mode.add_argument("--analyze", action="store_true", help="Print source metadata as JSON without creating a bundle or running KNS")
    parser.add_argument("--format", choices=("csv", "json", "pcap", "pcapng"))
    parser.add_argument("--address-map", type=Path, help="Required for PCAP/PCAPNG: explicit IP-to-model-node JSON mapping (v1.0 IPv4; v1.1 IPv4/IPv6)")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--keep-flow-labels", action="store_true", help="Explicitly retain original flow labels in local metadata")
    parser.add_argument("--exe", type=Path, help="Optionally execute the imported profile and retain a portable manifest")
    args = parser.parse_args(argv)
    try:
        if args.analyze and args.exe:
            raise ValueError("--analyze cannot be combined with --exe")
        topology = json_input(read(args.topology))
        address_map = json_input(read(args.address_map)) if args.address_map else None
        plan, metadata = convert(read(args.trace), args.format or args.trace.suffix.lower().lstrip("."), topology, args.seed, args.keep_flow_labels, address_map)
        if args.analyze:
            print(json.dumps({"schema_version": "1.0", "status": "analyzed", "scope": "source_record_metadata",
                "simulation_executed": False, "topology_validation": "structural_only", "metadata": metadata}, allow_nan=False))
            return 0
        if args.exe and not args.exe.is_file():
            raise ValueError("KNS executable does not exist")
        output = args.output.resolve()
        profile = {"schema_version": "1.0", "name": "imported-trace", "topology": str(output / "topology.json"),
            "seeds": [args.seed], "traffic_plan": plan, "max_cases": 1,
            "experiments": [{"name": "replay", "bandwidth": topology["links"][0]["bandwidth"]}]}
        experiment_profile.validate_profile(profile, topology)
        output.mkdir(parents=True, exist_ok=False)
        for name, value in (("topology.json", topology), ("traffic.json", plan), ("trace-metadata.json", metadata), ("profile.json", profile)):
            (output / name).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        if args.exe:
            return experiment_profile.main(["--profile", str(output / "profile.json"), "--exe", str(args.exe.resolve()), "--output", str(output / "experiment")])
        print("Trace bundle: " + str(output))
    except (OSError, ValueError, TypeError, KeyError, OverflowError, RecursionError, csv.Error) as error:
        if args.analyze:
            print(json.dumps({"schema_version": "1.0", "status": "invalid_input", "error": str(error)}), file=sys.stderr)
        else:
            print("[ERROR] " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
