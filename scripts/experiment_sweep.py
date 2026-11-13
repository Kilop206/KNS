"""Bounded deterministic Cartesian expansion of link parameter lists/ranges."""
from decimal import Decimal, ROUND_FLOOR
import itertools
import math
import traffic_config


PARAMETERS = {"bandwidth", "delay", "loss", "queue_capacity"}


def finite_number(value):
    try:
        return type(value) in (int, float) and math.isfinite(value)
    except OverflowError:
        return False


def choices(specification, limit: int) -> list:
    if isinstance(specification, list):
        if not specification or len(specification) > limit:
            raise ValueError("Sweep list must be non-empty and fit max_cases")
        values = specification
    elif isinstance(specification, dict) and set(specification) == {"start", "stop", "step"}:
        raw = [specification[key] for key in ("start", "stop", "step")]
        if any(not finite_number(value) for value in raw):
            raise ValueError("Sweep interval values must be finite numbers")
        start, stop, step = (Decimal(str(value)) for value in raw)
        if step <= 0 or stop < start:
            raise ValueError("Sweep intervals require step > 0 and stop >= start")
        count = int(((stop - start) / step).to_integral_value(rounding=ROUND_FLOOR)) + 1
        if count > limit:
            raise ValueError("Sweep interval exceeds max_cases")
        values = []
        for index in range(count):
            value = start + step * index
            values.append(int(value) if value == value.to_integral_value() else float(value))
    else:
        raise ValueError("Sweep dimension must be a list or {start, stop, step}")
    if any(not finite_number(value) for value in values):
        raise ValueError("Sweep choices must be finite numbers")
    if len(set(values)) != len(values):
        raise ValueError("Sweep choices must be distinct")
    return values


def expand(sweep: dict, limit: int) -> list[dict]:
    if not isinstance(sweep, dict) or not sweep or set(sweep) - PARAMETERS - {"traffic"}:
        raise ValueError("sweep must contain supported link/traffic parameters")
    keys = sorted(set(sweep) - {"traffic"})
    dimensions = [choices(sweep[key], limit) for key in keys]
    traffic = sweep.get("traffic", [])
    if not isinstance(traffic, list) or len(traffic) > 16 or ("traffic" in sweep and not traffic):
        raise ValueError("Traffic sweep must contain 1..16 axes")
    seen = set()
    for axis in traffic:
        if not isinstance(axis, dict) or set(axis) != {"flow_id", "parameter", "values"}:
            raise ValueError("Traffic sweep axis requires flow_id, parameter and values")
        flow_id = traffic_config.integer(axis["flow_id"], 1, 2**32 - 1, "traffic flow_id")
        parameter = axis["parameter"]
        if not isinstance(parameter, str) or parameter not in traffic_config.SWEEP_PARAMETERS or (flow_id, parameter) in seen:
            raise ValueError("Invalid or repeated traffic sweep parameter")
        seen.add((flow_id, parameter))
        dimensions.append(choices(axis["values"], limit))
    count = math.prod(map(len, dimensions))
    if count > limit:
        raise ValueError(f"Sweep has {count} variants, exceeding max_cases={limit}")
    variants = []
    for index, values in enumerate(itertools.product(*dimensions), 1):
        variant = {"name": f"sweep-{index:04d}", **dict(zip(keys, values[:len(keys)]))}
        if traffic:
            variant["traffic_overrides"] = [{"flow_id": axis["flow_id"], "parameter": axis["parameter"], "value": value}
                for axis, value in zip(traffic, values[len(keys):])]
        variants.append(variant)
    return variants
