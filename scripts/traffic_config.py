"""Preflight the bounded abstract Traffic Lab plan; no packets or sockets created."""
import math


def integer(value, low, high, label):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{label} must be an integer in [{low}, {high}]")
    return value


def number(value, low, high, label):
    try:
        valid = type(value) in (int, float) and math.isfinite(value) and low <= value <= high
    except OverflowError:
        valid = False
    if not valid:
        raise ValueError(f"{label} must be finite in [{low}, {high}]")
    return value


def validate(plan, topology):
    if not isinstance(plan, dict) or set(plan) != {"schema_version", "seed", "max_packets", "flows"}:
        raise ValueError("Traffic plan requires only schema_version, seed, max_packets and flows")
    if plan["schema_version"] != "1.0":
        raise ValueError("Unsupported traffic schema_version")
    integer(plan["seed"], 0, 2**64 - 1, "traffic seed")
    integer(plan["max_packets"], 1, 1000000, "max_packets")
    flows = plan["flows"]
    if not isinstance(flows, list) or not 1 <= len(flows) <= 256:
        raise ValueError("Traffic requires between 1 and 256 flows")
    ids = set()
    for flow in flows:
        required = {"id", "source", "destination", "start_s", "end_s", "wire_bytes", "schedule"}
        if not isinstance(flow, dict) or not required <= set(flow) or set(flow) - required - {"priority", "ttl_hops", "deadline_s"}:
            raise ValueError("Flow contains missing or unsupported fields")
        identifier = integer(flow["id"], 1, 2**32 - 1, "flow id")
        if identifier in ids:
            raise ValueError("Traffic flow IDs must be unique")
        ids.add(identifier)
        for field in ("source", "destination"):
            integer(flow[field], 0, len(topology["nodes"]) - 1, field)
        if flow["source"] == flow["destination"]:
            raise ValueError("Traffic endpoints must differ")
        start = number(flow["start_s"], 0, 1e6, "start_s")
        end = number(flow["end_s"], 0, 1e6, "end_s")
        if start >= end:
            raise ValueError("Traffic end_s must be after start_s")
        size = integer(flow["wire_bytes"], 1, 1048576, "wire_bytes")
        integer(flow.get("priority", 0), 0, 7, "priority")
        integer(flow.get("ttl_hops", 64), 1, 4096, "ttl_hops")
        if "deadline_s" in flow:
            number(flow["deadline_s"], 1e-9, 1e6, "deadline_s")
        schedule = flow["schedule"]
        fields = {"periodic": {"interval_s"}, "cbr": {"rate_bps"}, "poisson": {"rate_pps"},
                  "uniform": {"min_interval_s", "max_interval_s"},
                  "burst": {"burst_packets", "burst_interval_s", "packet_interval_s"}, "custom": {"offsets_s"}}
        if not isinstance(schedule, dict) or not isinstance(schedule.get("kind"), str) or schedule["kind"] not in fields or set(schedule) != {"kind"} | fields[schedule["kind"]]:
            raise ValueError("Traffic schedule kind/fields are invalid")
        kind = schedule["kind"]
        if kind == "custom":
            offsets = schedule["offsets_s"]
            if not isinstance(offsets, list) or not 1 <= len(offsets) <= 10000:
                raise ValueError("Custom traffic needs 1..10000 offsets")
            previous = -1
            for offset in offsets:
                number(offset, 0, end - start, "custom offset")
                if offset < previous or start + offset >= end:
                    raise ValueError("Custom offsets must be ordered and before end_s")
                previous = offset
        elif kind in ("cbr", "poisson"):
            rate = number(schedule["rate_bps" if kind == "cbr" else "rate_pps"], 1e-9, 1e15, "traffic rate")
            if kind == "cbr" and size * 8 / rate < 1e-9:
                raise ValueError("CBR interval must be at least 1 ns")
        else:
            for key in fields[kind] - {"burst_packets"}:
                number(schedule[key], 0 if key == "packet_interval_s" else 1e-9, 1e6, key)
            if kind == "uniform" and schedule["max_interval_s"] < schedule["min_interval_s"]:
                raise ValueError("Uniform intervals are reversed")
            if kind == "burst":
                n = integer(schedule["burst_packets"], 1, 1000000, "burst_packets")
                if (n - 1) * schedule["packet_interval_s"] >= schedule["burst_interval_s"]:
                    raise ValueError("Bursts must not overlap")


def validate_report(report, plan, complete=True):
    if not isinstance(report, dict) or report.get("schema_version") != "1.0" or report.get("config") != plan:
        raise ValueError("Traffic report does not match its plan")
    number(report.get("simulation_time_s"), 0, float("inf"), "simulation_time_s")
    integer(report.get("pending_events"), 0, 2**64 - 1, "pending_events")
    for key in ("generation_limit_reached", "generation_pending"):
        if type(report.get(key)) is not bool:
            raise ValueError(f"{key} must be a boolean")
    totals = {key: 0 for key in ("generated", "delivered", "dropped", "pending")}
    flows = report.get("flows")
    if not isinstance(flows, list) or len(flows) != len(plan["flows"]):
        raise ValueError("Traffic report is missing flows")
    generation_pending = False
    for observed, config in zip(flows, plan["flows"]):
        if not isinstance(observed, dict) or observed.get("config") != config or observed.get("id") != config["id"]:
            raise ValueError("Traffic report flow/config mismatch")
        integer(observed.get("stream_seed"), 0, 2**64 - 1, "stream_seed")
        for key in totals:
            totals[key] += integer(observed.get(key), 0, plan["max_packets"], "flow." + key)
        if observed["generated"] != observed["delivered"] + observed["dropped"] + observed["pending"]:
            raise ValueError("Traffic flow does not conserve packet outcomes")
        if integer(observed.get("delivered_wire_bytes"), 0, 2**64 - 1, "delivered_wire_bytes") != observed["delivered"] * config["wire_bytes"]:
            raise ValueError("Traffic delivered bytes contradict packet size")
        on_time = integer(observed.get("delivered_on_time"), 0, plan["max_packets"], "delivered_on_time")
        late = integer(observed.get("delivered_late"), 0, plan["max_packets"], "delivered_late")
        if on_time + late != (observed["delivered"] if "deadline_s" in config else 0):
            raise ValueError("Traffic deadline counts do not conserve deliveries")
        if type(observed.get("generation_pending")) is not bool:
            raise ValueError("Flow generation_pending must be a boolean")
        generation_pending |= observed["generation_pending"]
    for key, value in totals.items():
        if integer(report.get(key), 0, plan["max_packets"], key) != value:
            raise ValueError("Traffic global counts do not conserve flow outcomes")
    if generation_pending != report["generation_pending"]:
        raise ValueError("Traffic generation status contradicts flows")
    if complete and (totals["pending"] or generation_pending or report["pending_events"] or report["generation_limit_reached"]):
        raise ValueError("Traffic experiment is incomplete or its generation budget truncated offered load")
