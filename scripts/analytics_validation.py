"""Conservation, domain and scope checks before accepting or plotting metrics."""
import math
import re


def count(value, name):
    if type(value) is not int or not 0 <= value <= 2**64 - 1:
        raise ValueError(f"{name} must be a nonnegative 64-bit integer")
    return value


def number(value, name, nullable=False):
    if value is None and nullable:
        return None
    try:
        valid = type(value) in (int, float) and math.isfinite(value) and value >= 0
    except OverflowError:
        valid = False
    if not valid:
        raise ValueError(f"{name} must be finite and nonnegative")
    return value


def statistics(value, name):
    if not isinstance(value, dict):
        raise ValueError(f"{name} must be statistics")
    n = count(value.get("count"), name + ".count")
    retained = count(value.get("retained_samples"), name + ".retained_samples")
    discarded = count(value.get("discarded_samples"), name + ".discarded_samples")
    if retained + discarded != n or bool(retained) != bool(n):
        raise ValueError(f"{name} retention counters do not conserve observations")
    expected = "most_recent_retained_deliveries" if discarded else "all_observed_deliveries"
    if value.get("percentile_scope") != expected:
        raise ValueError(f"{name} percentile scope contradicts retention")
    fields = ("mean_s", "min_s", "max_s", "median_s", "p95_s", "p99_s")
    for field in fields:
        if n:
            number(value.get(field), name + "." + field)
        elif value.get(field) is not None:
            raise ValueError(f"{name} has statistics without observations")
    if n:
        low, high = value["min_s"], value["max_s"]
        if not low <= value["mean_s"] <= high or not low <= value["median_s"] <= value["p95_s"] <= value["p99_s"] <= high:
            raise ValueError(f"{name} moments/percentiles violate bounds")
    if n > 1:
        number(value.get("sample_stddev_s"), name + ".sample_stddev_s")
    elif value.get("sample_stddev_s") is not None:
        raise ValueError(f"{name} has sample deviation without two observations")
    histogram = value.get("histogram")
    if not isinstance(histogram, dict):
        raise ValueError(f"{name} histogram is missing")
    edges, bins = histogram.get("upper_edges_s"), histogram.get("counts")
    if not isinstance(edges, list) or not edges or not isinstance(bins, list) or len(bins) != len(edges) + 1:
        raise ValueError(f"{name} histogram dimensions are invalid")
    if any(number(edge, name + ".histogram.edge") <= 0 for edge in edges) or any(a >= b for a, b in zip(edges, edges[1:])):
        raise ValueError(f"{name} histogram edges must increase")
    if sum(count(v, name + ".histogram.count") for v in bins) != n:
        raise ValueError(f"{name} histogram does not conserve observations")
    if histogram.get("scope") != "all_observed_deliveries":
        raise ValueError(f"{name} histogram scope is invalid")
    return n


def validate(report):
    pending = [report]
    while pending:
        value = pending.pop()
        if isinstance(value, dict):
            pending.extend(value.values())
        elif isinstance(value, list):
            pending.extend(value)
        elif type(value) is float and not math.isfinite(value):
            raise ValueError("Analytics contains a non-finite value")
    config = report["config"]
    horizon = number(report.get("collection_until_s"), "collection_until_s")
    if horizon != config["until_s"] or number(report.get("simulation_end_s"), "simulation_end_s") < horizon:
        raise ValueError("Analytics horizon contradicts the execution scope")
    for key in ("reservation_records_discarded", "untracked_link_transmissions"):
        count(report.get(key), key)
    def retained(stats, maximum):
        if stats["retained_samples"] != min(stats["count"], maximum):
            raise ValueError("Statistics retention contradicts configuration")
    statistics(report.get("data_latency"), "data_latency")
    retained(report["data_latency"], config.get("latency_retention", 4096))
    if "data_latency_variation" in report:
        statistics(report["data_latency_variation"], "data_latency_variation")
        retained(report["data_latency_variation"], config.get("latency_retention", 4096))
    delivered = count(report.get("packets_delivered_in_scope"), "packets_delivered_in_scope")
    count(report.get("wire_bytes_delivered_in_scope"), "wire_bytes_delivered_in_scope")
    if report["data_latency"]["count"] > delivered:
        raise ValueError("DATA deliveries exceed total observed deliveries")
    for key, overflow in (("flow_groups", "ungrouped_flow_deliveries"), ("destination_node_groups", "ungrouped_node_deliveries")):
        groups = report.get(key)
        if not isinstance(groups, list):
            raise ValueError(f"{key} must be an array")
        if len(groups) > config.get("max_groups", 128):
            raise ValueError("Group count exceeds configured retention")
        missing = count(report.get(overflow), overflow)
        keys, total, data_total, byte_total = set(), missing, 0, 0
        for group in groups:
            if not isinstance(group, dict) or not isinstance(group.get("key"), str) or group["key"] in keys:
                raise ValueError(f"{key} contains invalid or duplicate keys")
            keys.add(group["key"])
            pattern = r"[0-9]+(?::[0-9]+){4}" if key == "flow_groups" else r"[0-9]+"
            if re.fullmatch(pattern, group["key"]) is None:
                raise ValueError("Group key does not match its declared dimensions")
            packets = count(group.get("packets_delivered"), key + ".packets_delivered")
            total += packets
            byte_total += count(group.get("wire_bytes_delivered"), key + ".wire_bytes_delivered")
            observed_data = statistics(group.get("data_latency"), key + ".data_latency")
            data_total += observed_data
            if observed_data > packets:
                raise ValueError("Group DATA count exceeds packet count")
            retained(group["data_latency"], config.get("group_retention", 128))
            if "data_latency_variation" in group:
                pairs = statistics(group["data_latency_variation"], key + ".data_latency_variation")
                if pairs != max(0, group["data_latency"]["count"] - 1):
                    raise ValueError("Flow variation count differs from consecutive DATA pairs")
                retained(group["data_latency_variation"], config.get("group_retention", 128))
        if total != delivered:
            raise ValueError(f"{key} counts do not conserve deliveries")
        if data_total > report["data_latency"]["count"] or byte_total > report["wire_bytes_delivered_in_scope"]:
            raise ValueError(f"{key} exceeds global DATA/byte totals")
        if missing == 0 and (data_total != report["data_latency"]["count"] or byte_total != report["wire_bytes_delivered_in_scope"]):
            raise ValueError(f"{key} does not conserve global DATA/byte totals")
    if "data_latency_variation" in report:
        total = sum(group.get("data_latency_variation", {}).get("count", 0) for group in report["flow_groups"])
        if report["data_latency_variation"]["count"] != total:
            raise ValueError("Latency variation does not conserve tracked-flow pairs")
    previous = None
    for sample in report["samples"]:
        at = number(sample.get("at_s"), "sample.at_s")
        start = number(sample.get("from_s"), "sample.from_s")
        duration = number(sample.get("duration_s"), "sample.duration_s")
        if start > at or not math.isclose(duration, at - start, rel_tol=1e-12, abs_tol=1e-15):
            raise ValueError("Sample interval is inconsistent")
        if previous is not None and start != previous["at_s"]:
            raise ValueError("Retained sample intervals are not contiguous")
        for key in ("packets_sent", "packets_delivered", "packets_lost", "queue_overflow_drops", "red_early_drops"):
            count(sample.get(key), "sample." + key)
            if previous is not None and sample[key] < previous[key]:
                raise ValueError("Sample packet counters moved backwards")
        for rate, counter in (("throughput_pps", "packets_delivered"), ("first_hop_admission_pps", "packets_sent"), ("drops_per_s", "packets_lost")):
            value = number(sample.get(rate), "sample." + rate, nullable=duration == 0)
            if duration == 0 and value is not None:
                raise ValueError("Zero-duration rates must be null")
            if duration and previous is not None:
                expected = (sample[counter] - previous[counter]) / duration
                if not math.isclose(value, expected, rel_tol=1e-9, abs_tol=1e-12):
                    raise ValueError("Sample rate contradicts cumulative counters")
        statistics(sample.get("data_latency"), "sample.data_latency")
        retained(sample["data_latency"], min(config.get("latency_retention", 4096), 256))
        if "data_latency_variation" in sample:
            statistics(sample["data_latency_variation"], "sample.data_latency_variation")
            retained(sample["data_latency_variation"], min(config.get("latency_retention", 4096), 256))
        links = sample.get("links")
        if not isinstance(links, list):
            raise ValueError("Sample links must be an array")
        indices = set()
        for link in links:
            if not isinstance(link, dict):
                raise ValueError("Link sample must be an object")
            index = count(link.get("link_index"), "link_index")
            if index in indices:
                raise ValueError("Duplicate link sample")
            indices.add(index)
            if any(type(link.get(flag)) is not bool for flag in ("utilization_defined", "utilization_complete")):
                raise ValueError("Utilization coverage flags must be booleans")
            utilization = link.get("utilization")
            if not isinstance(utilization, dict) or set(utilization) != {"ab", "ba", "shared"}:
                raise ValueError("Invalid utilization slots")
            for fraction in [*utilization.values(), link.get("max_queue_fraction")]:
                if fraction is not None and number(fraction, "link fraction") > 1:
                    raise ValueError("Link fraction exceeds one")
            if not link.get("utilization_defined") and any(v is not None for v in utilization.values()):
                raise ValueError("Undefined utilization must not contain measurements")
            if link["utilization_defined"] and not any(v is not None for v in utilization.values()):
                raise ValueError("Defined utilization is missing measurements")
        previous = sample
    recommendations = report.get("recommendations")
    if not isinstance(recommendations, list):
        raise ValueError("Recommendations must be an array")
    for item in recommendations:
        if not isinstance(item, dict) or any(not isinstance(item.get(key), str) for key in ("rule", "classification", "recommendation", "limitations")) or not isinstance(item.get("evidence"), dict):
            raise ValueError("Recommendation lacks evidence or explanation")
