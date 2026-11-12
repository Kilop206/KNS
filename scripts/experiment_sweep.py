"""Bounded deterministic Cartesian expansion of link parameter lists/ranges."""
from decimal import Decimal, ROUND_FLOOR
import itertools
import math


PARAMETERS = {"bandwidth", "delay", "loss", "queue_capacity"}


def choices(specification, limit: int) -> list:
    if isinstance(specification, list):
        if not specification or len(specification) > limit:
            raise ValueError("Sweep list must be non-empty and fit max_cases")
        values = specification
    elif isinstance(specification, dict) and set(specification) == {"start", "stop", "step"}:
        raw = [specification[key] for key in ("start", "stop", "step")]
        if any(type(value) not in (int, float) or not math.isfinite(value) for value in raw):
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
    if any(type(value) not in (int, float) or not math.isfinite(value) for value in values):
        raise ValueError("Sweep choices must be finite numbers")
    if len(set(values)) != len(values):
        raise ValueError("Sweep choices must be distinct")
    return values


def expand(sweep: dict, limit: int) -> list[dict]:
    if not isinstance(sweep, dict) or not sweep or set(sweep) - PARAMETERS:
        raise ValueError("sweep must contain supported link parameters")
    keys = sorted(sweep)
    dimensions = [choices(sweep[key], limit) for key in keys]
    count = math.prod(map(len, dimensions))
    if count > limit:
        raise ValueError(f"Sweep has {count} variants, exceeding max_cases={limit}")
    return [{"name": f"sweep-{index:04d}", **dict(zip(keys, values))}
            for index, values in enumerate(itertools.product(*dimensions), 1)]
