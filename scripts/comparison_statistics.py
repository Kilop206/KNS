"""Descriptive paired statistics; conditional t intervals without optional packages."""
import math
import statistics

# NIST/SEMATECH e-Handbook, t(0.975, df), rounded to 3 decimals.
# https://www.itl.nist.gov/div898/handbook/eda/section3/eda3672.htm
T975 = (12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228,
        2.201, 2.179, 2.160, 2.145, 2.131, 2.120, 2.110, 2.101, 2.093, 2.086,
        2.080, 2.074, 2.069, 2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042)
DIRECTIONS = {"delivery_rate": 1, "loss_rate": -1, "throughput_pps": 1,
              "avg_latency_s": -1, "simulation_duration_s": 0,
              "queue_overflow_drops": -1, "red_early_drops": 0}


def direction(metric: str, delta: float | None) -> str:
    if delta is None:
        return "unavailable"
    if delta == 0:
        return "unchanged"
    if DIRECTIONS[metric] == 0:
        return "context_dependent"
    return "improvement" if DIRECTIONS[metric] * delta > 0 else "regression"


def relative(delta: float | None, baseline: float | None) -> float | None:
    if delta is None or baseline in (None, 0):
        return None
    value = (delta / baseline) * 100
    if not math.isfinite(value):
        raise ValueError("Relative delta exceeds supported numeric range")
    return value


def summarize(metric: str, pairs: list[dict]) -> dict:
    valid = [pair for pair in pairs if pair[f"{metric}_delta"] is not None]
    values = [pair[f"{metric}_delta"] for pair in valid]
    percentages = [pair[f"{metric}_delta_percent"] for pair in valid
                   if pair[f"{metric}_delta_percent"] is not None]
    n = len(values)
    mean = statistics.fmean(values) if n else None
    deviation = statistics.stdev(values) if n >= 2 else None
    interval = None
    reason = "insufficient_samples" if n < 2 else "no_observed_variation"
    if deviation is not None and deviation > 0:
        half = T975[min(n - 1, 30) - 1] * (deviation / math.sqrt(n))
        low, high = mean - half, mean + half
        if not all(map(math.isfinite, (low, high))):
            raise ValueError("Confidence interval exceeds supported numeric range")
        interval = {"low": low, "high": high}
        reason = "conditional_on_independent_normal_paired_deltas"
    return {
        "sample_count": n, "mean_delta": mean,
        "min_delta": min(values) if n else None, "max_delta": max(values) if n else None,
        "median_delta": statistics.median(values) if n else None,
        "sample_stddev_delta": deviation,
        "baseline_mean": statistics.fmean(pair[f"{metric}_baseline"] for pair in valid) if n else None,
        "experiment_mean": statistics.fmean(pair[f"{metric}_experiment"] for pair in valid) if n else None,
        "mean_delta_percent": statistics.fmean(percentages) if percentages else None,
        "relative_sample_count": len(percentages),
        "direction": direction(metric, mean),
        "conditional_ci95": interval, "ci_status": reason,
    }
