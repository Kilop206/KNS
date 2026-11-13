"""Preflight and execution-evidence checks for optional native Analytics collection."""
import math

LIMITS = {"max_samples": (2048, 10000), "latency_retention": (4096, 100000),
          "max_groups": (128, 1024), "group_retention": (128, 1024),
          "max_reservations": (65536, 1000000)}


def times(config: dict) -> list[float]:
    interval, until = config["interval_s"], config["until_s"]
    return [0, *(i * interval for i in range(1, math.floor(until / interval) + 1) if i * interval < until),
            *([until] if until > 0 else [])]


def validate(config: dict, topology: dict | None = None) -> None:
    if not isinstance(config, dict) or set(config) - ({"schema_version", "interval_s", "until_s"} | set(LIMITS)):
        raise ValueError("Analytics config contains unsupported fields")
    if config.get("schema_version") != "1.0":
        raise ValueError("Unsupported analytics config version")
    for key in ("until_s", "interval_s"):
        value = config.get(key)
        if type(value) not in (int, float) or not math.isfinite(value) or value < 0:
            raise ValueError(f"Analytics {key} must be finite and nonnegative")
    if config["interval_s"] <= 0 or not math.isfinite(config["until_s"] / config["interval_s"]) or config["until_s"] / config["interval_s"] > 10000:
        raise ValueError("Analytics requires a positive interval and at most 10000 intervals")
    for key, (default, maximum) in LIMITS.items():
        value = config.get(key, default)
        if type(value) is not int or not 1 <= value <= maximum:
            raise ValueError(f"Analytics {key} must be an integer in [1,{maximum}]")
    retained = min(config.get("max_samples", 2048), len(times(config)))
    if topology is not None and retained * len(topology["links"]) > 1000000:
        raise ValueError("Analytics retention exceeds one million link samples")


def validate_report(report: dict, config: dict) -> None:
    validate(config)
    if not isinstance(report, dict) or report.get("schema_version") != "1.0" or report.get("config") != config:
        raise ValueError("Analytics report does not match its requested config")
    expected = times(config)
    retained = expected[-config.get("max_samples", 2048):]
    if type(report.get("samples_seen")) is not int or type(report.get("pending_events")) is not int or report.get("samples_seen") != len(expected) or report.get("pending_events") != 0:
        raise ValueError("Analytics observations are incomplete")
    samples = report.get("samples")
    if not isinstance(samples, list) or any(not isinstance(item, dict) for item in samples):
        raise ValueError("Analytics samples must be objects")
    if type(report.get("samples_discarded")) is not int or [item.get("at_s") for item in samples] != retained or report.get("samples_discarded") != len(expected) - len(retained):
        raise ValueError("Analytics sample timestamps or retention counters do not match")
    import analytics_validation
    analytics_validation.validate(report)
