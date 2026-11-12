"""Preflight validation for the versioned, link-only discrete-event Fault Lab."""
import math


def validate(plan: dict, topology: dict) -> None:
    allowed = {"schema_version", "in_flight_policy", "events", "probes", "observe_at"}
    if not isinstance(plan, dict) or set(plan) != allowed:
        raise ValueError("Fault plan requires exactly schema_version, in_flight_policy, events, probes, observe_at")
    if plan["schema_version"] != "1.0" or plan["in_flight_policy"] != "complete_accepted_hops":
        raise ValueError("Unsupported fault plan version or in-flight policy")
    def number(value):
        return type(value) in (int, float) and math.isfinite(value) and value >= 0
    if not isinstance(plan["events"], list) or len(plan["events"]) > 10000:
        raise ValueError("Fault events must be a list with at most 10000 entries")
    times = {0}
    for event in plan["events"]:
        if not isinstance(event, dict):
            raise ValueError("Fault event must be an object")
        kind = event.get("type")
        parameter = kind in ("bandwidth_mbps", "delay_ms", "loss_probability")
        if kind not in ("link_down", "link_up", "bandwidth_mbps", "delay_ms", "loss_probability"):
            raise ValueError("Unsupported fault type")
        if set(event) != ({"at", "type", "link_index", "value"} if parameter else {"at", "type", "link_index"}):
            raise ValueError("Fault event fields do not match its type")
        link = event["link_index"]
        if type(link) is not int or not 0 <= link < len(topology["links"]) or not number(event["at"]):
            raise ValueError("Invalid fault timestamp or link_index")
        if parameter:
            value = event["value"]
            if not number(value) or kind == "bandwidth_mbps" and value == 0 or kind == "loss_probability" and value > 1:
                raise ValueError("Fault parameter value is outside its domain")
        times.add(event["at"])
    if not isinstance(plan["observe_at"], list) or len(plan["observe_at"]) > 10000 or not all(map(number, plan["observe_at"])):
        raise ValueError("observe_at must contain at most 10000 finite nonnegative timestamps")
    times.update(plan["observe_at"])
    if len(times) * len(topology["links"]) > 1000000:
        raise ValueError("Fault observations exceed one million link samples")
    probes = plan["probes"]
    nodes = topology.get("nodes")
    size = nodes if type(nodes) is int else len(nodes) if isinstance(nodes, list) else 0
    if not isinstance(probes, list) or len(probes) > 64:
        raise ValueError("Fault probes must be a list of at most 64 entries")
    for probe in probes:
        if not isinstance(probe, dict) or set(probe) != {"source", "destination"}:
            raise ValueError("Probe requires source and destination")
        if any(type(v) is not int or not 0 <= v < size for v in probe.values()) or probe["source"] == probe["destination"]:
            raise ValueError("Probe endpoints must be distinct valid nodes")


def validate_report(report: dict, plan: dict) -> None:
    """Require evidence that all planned faults/observations actually executed."""
    if not isinstance(report, dict) or report.get("schema_version") != "1.0" or report.get("plan") != plan:
        raise ValueError("Fault report does not match the requested plan")
    changes = report.get("changes")
    expected = sorted(plan["events"], key=lambda event: event["at"])
    if not isinstance(changes, list) or len(changes) != len(expected):
        raise ValueError("Fault report is missing applied events")
    for actual, event in zip(changes, expected):
        if not isinstance(actual, dict) or actual.get("status") != "applied" or actual.get("event") != event or actual.get("at") != event["at"]:
            raise ValueError("Fault report event sequence differs from the plan")
    times = sorted({0, *plan["observe_at"], *(event["at"] for event in expected)})
    observations = report.get("observations")
    if not isinstance(observations, list) or any(not isinstance(item, dict) for item in observations):
        raise ValueError("Fault observations must be objects")
    if [item.get("at") for item in observations] != times or report.get("pending_events") != 0:
        raise ValueError("Fault observations are incomplete")
