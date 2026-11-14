"""Preflight the bounded abstract Traffic Lab plan; no packets or sockets created."""
import copy
import math

SWEEP_PARAMETERS = {"wire_bytes", "priority", "ttl_hops", "deadline_s", "start_s", "end_s",
    "schedule.interval_s", "schedule.rate_bps", "schedule.rate_pps", "schedule.min_interval_s", "schedule.max_interval_s",
    "schedule.burst_packets", "schedule.burst_interval_s", "schedule.packet_interval_s",
    "application.response_wire_bytes", "application.processing_delay_s", "application.deadline_s"}


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


def validate(plan, topology=None):
    required = {"schema_version", "seed", "max_packets", "flows"}
    if not isinstance(plan, dict) or not required <= set(plan) or set(plan) - required - {"replay_events"}:
        raise ValueError("Traffic plan requires schema_version, seed, max_packets, flows and optional replay_events")
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
        if not isinstance(flow, dict) or not required <= set(flow) or set(flow) - required - {"priority", "ttl_hops", "deadline_s", "application"}:
            raise ValueError("Flow contains missing or unsupported fields")
        identifier = integer(flow["id"], 1, 2**32 - 1, "flow id")
        if identifier in ids:
            raise ValueError("Traffic flow IDs must be unique")
        ids.add(identifier)
        for field in ("source", "destination"):
            integer(flow[field], 0, len(topology["nodes"]) - 1 if topology is not None else 2**31 - 1, field)
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
        if "application" in flow:
            app = flow["application"]
            kinds = {"request_response", "http", "api", "database", "messaging", "telemetry", "microservice", "streaming"}
            if not isinstance(app, dict) or "kind" not in app or set(app) - {"kind", "response_wire_bytes", "processing_delay_s", "deadline_s"} or not isinstance(app["kind"], str) or app["kind"] not in kinds:
                raise ValueError("Invalid application abstraction")
            response = "response_wire_bytes" in app
            if (app["kind"] in {"request_response", "http", "api", "database", "microservice"} and not response) or response != ("processing_delay_s" in app) or (not response and "deadline_s" in app):
                raise ValueError("Application response configuration is incomplete")
            if response:
                integer(app["response_wire_bytes"], 1, 1048576, "response_wire_bytes")
                number(app["processing_delay_s"], 0, 1e6, "processing_delay_s")
                if "deadline_s" in app:
                    number(app["deadline_s"], 1e-9, 1e6, "operation deadline_s")
        schedule = flow["schedule"]
        fields = {"periodic": {"interval_s"}, "cbr": {"rate_bps"}, "poisson": {"rate_pps"},
                  "uniform": {"min_interval_s", "max_interval_s"},
                  "burst": {"burst_packets", "burst_interval_s", "packet_interval_s"}, "custom": {"offsets_s"}, "replay": set()}
        if not isinstance(schedule, dict) or not isinstance(schedule.get("kind"), str) or schedule["kind"] not in fields or set(schedule) != {"kind"} | fields[schedule["kind"]]:
            raise ValueError("Traffic schedule kind/fields are invalid")
        kind = schedule["kind"]
        if "replay_events" in plan and kind != "replay":
            raise ValueError("Replay plans require only replay schedules")
        if kind == "replay":
            if "replay_events" not in plan or "application" in flow:
                raise ValueError("Replay schedules require replay_events and do not synthesize application responses")
        elif kind == "custom":
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
    if "replay_events" in plan:
        events = plan["replay_events"]
        if not isinstance(events, list) or not 1 <= len(events) <= 10000:
            raise ValueError("Replay requires 1..10000 events")
        by_id = {flow["id"]: flow for flow in flows}
        previous, referenced = -1, set()
        for event in events:
            if not isinstance(event, dict) or set(event) != {"at_s", "flow_id"}:
                raise ValueError("Replay event requires at_s and flow_id")
            flow_id = integer(event["flow_id"], 1, 2**32 - 1, "replay flow_id")
            if flow_id not in by_id:
                raise ValueError("Replay selects an unknown flow")
            at = number(event["at_s"], 0, 1e6, "replay at_s")
            flow = by_id[flow_id]
            if at < previous or not flow["start_s"] <= at < flow["end_s"]:
                raise ValueError("Replay times must be ordered and within each flow window")
            previous = at; referenced.add(flow_id)
        if referenced != ids:
            raise ValueError("Replay must reference every configured flow")


def validate_report(report, plan, complete=True):
    validate(plan)
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
        app = config.get("application", {})
        has_response = "response_wire_bytes" in app
        responses_delivered = 0
        if has_response or "source_generated" in observed:
            stage = {key: integer(observed.get(key), 0, plan["max_packets"], key) for key in
                     ("source_generated", "responses_generated", "responses_delivered", "responses_dropped",
                      "operations_started", "operations_completed", "operations_failed", "operations_pending",
                      "operations_on_time", "operations_late")}
            if stage["source_generated"] + stage["responses_generated"] != observed["generated"] or stage["responses_delivered"] + stage["responses_dropped"] > stage["responses_generated"]:
                raise ValueError("Traffic source/response stages do not conserve packets")
            if stage["responses_delivered"] > observed["delivered"] or stage["responses_dropped"] > observed["dropped"] or stage["responses_generated"] > observed["delivered"] - stage["responses_delivered"]:
                raise ValueError("Response stages exceed observed request/delivery outcomes")
            if observed["delivered"] - stage["responses_delivered"] + observed["dropped"] - stage["responses_dropped"] > stage["source_generated"]:
                raise ValueError("Source terminal outcomes exceed source generations")
            if not has_response and (stage["responses_generated"] or any(stage[k] for k in stage if k.startswith("operations_"))):
                raise ValueError("One-way traffic has response/operation observations")
            started = stage["source_generated"] if has_response else 0
            failed = observed["dropped"] if has_response else 0
            if (stage["operations_started"], stage["operations_completed"], stage["operations_failed"]) != (started, stage["responses_delivered"], failed) or started != stage["operations_completed"] + failed + stage["operations_pending"]:
                raise ValueError("Application operations do not conserve outcomes")
            if stage["operations_on_time"] + stage["operations_late"] != (stage["operations_completed"] if "deadline_s" in app else 0):
                raise ValueError("Application deadline counts contradict completed operations")
            from analytics_validation import statistics
            latency = observed.get("operation_latency")
            if statistics(latency, "operation_latency") != stage["operations_completed"] or latency["retained_samples"] != min(stage["operations_completed"], 256):
                raise ValueError("Application latency statistics contradict completed operations")
            if complete and stage["operations_pending"]:
                raise ValueError("Application operations remain pending")
            responses_delivered = stage["responses_delivered"]
        expected_bytes = (observed["delivered"] - responses_delivered) * config["wire_bytes"] + responses_delivered * app.get("response_wire_bytes", 0)
        if integer(observed.get("delivered_wire_bytes"), 0, 2**64 - 1, "delivered_wire_bytes") != expected_bytes:
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


def apply_overrides(plan, overrides, topology=None):
    if not isinstance(overrides, list) or not 1 <= len(overrides) <= 16:
        raise ValueError("Traffic overrides require 1..16 parameters")
    validate(plan, topology)
    result = copy.deepcopy(plan)
    flows = {flow["id"]: flow for flow in result["flows"]}
    seen = set()
    for override in overrides:
        if not isinstance(override, dict) or set(override) != {"flow_id", "parameter", "value"}:
            raise ValueError("Traffic override requires flow_id, parameter and value")
        flow_id = integer(override["flow_id"], 1, 2**32 - 1, "override flow_id")
        parameter = override["parameter"]
        if not isinstance(parameter, str) or parameter not in SWEEP_PARAMETERS or (flow_id, parameter) in seen:
            raise ValueError("Invalid or repeated traffic override parameter")
        seen.add((flow_id, parameter))
        if flow_id not in flows:
            raise ValueError("Traffic override selects an unknown flow")
        target = flows[flow_id]
        path = parameter.split(".")
        if len(path) == 2:
            target = target.get(path[0])
            if not isinstance(target, dict) or path[1] not in target:
                raise ValueError("Traffic override cannot create or switch a schedule/application model")
        target[path[-1]] = override["value"]
    validate(result, topology)
    return result
