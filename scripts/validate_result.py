#!/usr/bin/env python3
"""Check explicit assertions against a complete Traffic Lab result, without rerunning it."""
import argparse
import hashlib
import json
import math
import operator
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

import experiment_manifest
import traffic_config

OPERATORS = {">=": operator.ge, "<=": operator.le, ">": operator.gt, "<": operator.lt, "==": operator.eq, "!=": operator.ne}
COUNTS = {"generated", "delivered", "dropped", "pending", "operations_started", "operations_completed", "operations_failed", "operations_pending"}
RATIOS = {"delivery_ratio", "packet_loss_ratio", "pending_ratio", "operation_completion_ratio", "on_time_operation_ratio", "on_time_packet_ratio"}
LATENCIES = {"operation_latency_mean_s": "mean_s", "operation_latency_p50_s": "median_s", "operation_latency_p95_s": "p95_s", "operation_latency_p99_s": "p99_s"}
METRICS = COUNTS | RATIOS | set(LATENCIES)


def read_json(path, limit):
    with path.open("rb") as source:
        data = source.read(limit + 1)
    if len(data) > limit:
        raise ValueError(f"{path.name} exceeds the input size limit")
    def reject(value):
        raise ValueError("Non-finite JSON numbers are unsupported: " + value)
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate JSON fields are unsupported")
            result[key] = value
        return result
    return json.loads(data.decode("utf-8"), parse_constant=reject, object_pairs_hook=unique_object), hashlib.sha256(data).hexdigest()


def validate_rules(rules):
    if not isinstance(rules, dict) or set(rules) != {"schema_version", "assertions"} or rules["schema_version"] not in ("1.0", "1.1"):
        raise ValueError("Rules require schema_version 1.0/1.1 and assertions")
    assertions = rules["assertions"]
    if not isinstance(assertions, list) or not 1 <= len(assertions) <= 1024:
        raise ValueError("Rules require 1..1024 assertions")
    names = set()
    for index, rule in enumerate(assertions, 1):
        required = {"metric", "operator"}
        if not isinstance(rule, dict) or not required <= set(rule) or set(rule) - required - {"value", "baseline_offset", "name", "flow_id"}:
            raise ValueError("Assertion has missing/unsupported fields")
        thresholds = set(rule) & {"value", "baseline_offset"}
        if len(thresholds) != 1 or ("baseline_offset" in rule and rules["schema_version"] != "1.1"):
            raise ValueError("Assertion requires exactly one threshold; baseline_offset requires rules 1.1")
        if not isinstance(rule["metric"], str) or rule["metric"] not in METRICS or not isinstance(rule["operator"], str) or rule["operator"] not in OPERATORS:
            raise ValueError("Unsupported assertion metric/operator")
        value = rule[next(iter(thresholds))]
        try:
            finite = type(value) in (int, float) and math.isfinite(value)
        except OverflowError:
            finite = False
        if not finite:
            raise ValueError("Assertion value must be finite")
        name = rule.get("name", f"assertion-{index}")
        if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,79}", name) or name in names:
            raise ValueError("Assertion names must be unique ASCII identifiers (1..80 characters)")
        names.add(name)
        if "flow_id" in rule:
            traffic_config.integer(rule["flow_id"], 1, 2**32 - 1, "assertion flow_id")
        if rule["metric"] in LATENCIES and "flow_id" not in rule:
            raise ValueError("Operation latency assertions require flow_id; per-flow percentiles cannot be averaged")


def measure(report, rule):
    flows = report["flows"]
    if "flow_id" in rule:
        flows = [flow for flow in flows if flow["id"] == rule["flow_id"]]
        if not flows:
            raise ValueError("Assertion selects an unknown flow_id")
    metric = rule["metric"]
    if metric in LATENCIES:
        statistics = flows[0].get("operation_latency")
        if statistics is None:
            return None, "seconds", "unavailable operation observations", {}
        field = LATENCIES[metric]
        scope = "all completed operations" if field == "mean_s" else statistics["percentile_scope"]
        return statistics[field], "seconds", scope, {key: statistics[key] for key in ("count", "retained_samples", "discarded_samples")}
    def total(key, selected=flows):
        if any(key not in flow for flow in selected):
            return None
        return sum(flow[key] for flow in selected)
    if metric in COUNTS:
        return total(metric), "count", "selected flows over complete execution", {}
    denominator_name = "generated"
    selected = flows
    if metric == "on_time_operation_ratio":
        selected = [f for f in flows if "deadline_s" in f["config"].get("application", {})]
        numerator, denominator = total("operations_on_time", selected), total("operations_started", selected)
        denominator_name = "operations_started_with_deadline"
    elif metric == "on_time_packet_ratio":
        selected = [f for f in flows if "deadline_s" in f["config"]]
        numerator, denominator = total("delivered_on_time", selected), total("generated", selected)
        denominator_name = "generated_packets_with_deadline"
    elif metric == "operation_completion_ratio":
        numerator, denominator = total("operations_completed"), total("operations_started")
        denominator_name = "operations_started"
    else:
        numerator = total({"delivery_ratio": "delivered", "packet_loss_ratio": "dropped", "pending_ratio": "pending"}[metric])
        denominator = total("generated")
    value = numerator / denominator if numerator is not None and denominator else None
    return value, "fraction", denominator_name, {"numerator": numerator, "denominator": denominator}


def offset_threshold(value, offset):
    threshold = value + offset
    try:
        finite = math.isfinite(threshold)
    except OverflowError:
        finite = False
    if not finite:
        raise ValueError("Baseline offset produces a non-finite threshold")
    return threshold


def evaluate(report, rules, baseline=None):
    traffic_config.validate_report(report, report["config"])
    validate_rules(rules)
    uses_baseline = any("baseline_offset" in rule for rule in rules["assertions"])
    configuration_hash = experiment_manifest.config_hash(report["config"])
    if uses_baseline:
        if not isinstance(baseline, dict) or "config" not in baseline:
            raise ValueError("baseline_offset assertions require a complete --baseline Traffic Lab report")
        traffic_config.validate_report(baseline, baseline["config"])
        if experiment_manifest.config_hash(baseline["config"]) != configuration_hash:
            raise ValueError("Baseline workload configuration must match exactly (including seed, flows and schedules)")
        if report.get("model") != baseline.get("model"):
            raise ValueError("Baseline model descriptions must match")
        if [flow["stream_seed"] for flow in report["flows"]] != [flow["stream_seed"] for flow in baseline["flows"]]:
            raise ValueError("Baseline recorded random stream seeds must match")
        primary = lambda data: [flow.get("source_generated", flow["generated"]) for flow in data["flows"]]
        if primary(report) != primary(baseline):
            raise ValueError("Baseline primary generated counts must match; offered cohorts differ")
    elif baseline is not None:
        raise ValueError("--baseline requires at least one baseline_offset assertion")
    checks = []
    for index, rule in enumerate(rules["assertions"], 1):
        value, unit, scope, evidence = measure(report, rule)
        expected = rule.get("value")
        reference = None
        reason = None
        if "baseline_offset" in rule:
            base_value, base_unit, base_scope, base_evidence = measure(baseline, rule)
            reference = {"observed": base_value, "offset": rule["baseline_offset"], "unit": base_unit,
                         "scope": base_scope, "evidence": base_evidence}
            if base_value is None:
                reason = "baseline metric unavailable or denominator zero"
            else:
                if unit != base_unit or (value is not None and scope != base_scope):
                    raise ValueError("Baseline metric units/scopes must match")
                expected = offset_threshold(base_value, rule["baseline_offset"])
        passed = expected is not None and value is not None and OPERATORS[rule["operator"]](value, expected)
        reason = reason or ("threshold satisfied" if passed else ("metric unavailable or denominator zero" if value is None else "threshold violated"))
        check = {"name": rule.get("name", f"assertion-{index}"), "metric": rule["metric"],
            "flow_id": rule.get("flow_id"), "operator": rule["operator"], "expected": expected,
            "observed": value, "unit": unit, "scope": scope, "evidence": evidence,
            "passed": passed, "reason": reason}
        if reference is not None:
            check["baseline"] = reference
        checks.append(check)
    result = {"schema_version": rules["schema_version"], "passed": all(check["passed"] for check in checks),
        "assertion_count": len(checks), "failed_assertions": sum(not check["passed"] for check in checks),
        "model": report.get("model"), "configuration_sha256": configuration_hash,
        "checks": checks, "limitations": "Checks assert declared simulation metrics; they do not authenticate inputs, establish causal differences or certify real-network SLAs."}
    if uses_baseline:
        result["baseline_configuration_sha256"] = experiment_manifest.config_hash(baseline["config"])
        result["limitations"] += " Baseline matching covers workload configuration, not topology/software provenance or statistical confidence. Delivered/completed populations can differ; examine loss/count/retention evidence alongside latency."
    return result


def write(result, output):
    (output / "validation.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    suite = ET.Element("testsuite", name="KNS scenario assertions", tests=str(result["assertion_count"]), failures=str(result["failed_assertions"]))
    lines = ["# KNS scenario assertions", "", "Passed: " + str(result["passed"]), "",
             "| Assertion | Metric | Observed | Rule | Result |", "| --- | --- | ---: | --- | --- |"]
    for check in result["checks"]:
        case = ET.SubElement(suite, "testcase", name=check["name"], classname="KNS.validation")
        detail = f"{check['metric']} flow={check['flow_id']} observed={check['observed']} {check['operator']} {check['expected']}; {check['scope']}; {check['reason']}"
        if "baseline" in check:
            detail += f"; baseline={check['baseline']['observed']} offset={check['baseline']['offset']}; baseline_scope={check['baseline']['scope']}"
        if not check["passed"]:
            ET.SubElement(case, "failure", message=check["reason"]).text = detail
        ET.SubElement(case, "system-out").text = detail
        lines.append(f"| {check['name']} | {check['metric']} | {check['observed']} | {check['operator']} {check['expected']} | {'pass' if check['passed'] else 'fail'} |")
    if any("baseline" in check for check in result["checks"]):
        lines.extend(["", "## Baseline thresholds", "", "| Assertion | Baseline | Offset | Effective threshold |", "| --- | ---: | ---: | ---: |"])
        for check in result["checks"]:
            if "baseline" in check:
                lines.append(f"| {check['name']} | {check['baseline']['observed']} | {check['baseline']['offset']} | {check['expected']} |")
    ET.ElementTree(suite).write(output / "validation.xml", encoding="utf-8", xml_declaration=True)
    (output / "validation.md").write_text("\n".join(lines) + "\n\n" + result["limitations"] + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--rules", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, help="Complete result with the identical workload config; required by rules 1.1 baseline_offset")
    parser.add_argument("--output", type=Path, required=True, help="New directory for JSON, JUnit XML and Markdown")
    args = parser.parse_args(argv)
    try:
        report, source_hash = read_json(args.report, 64 * 1024 * 1024)
        rules, rules_hash = read_json(args.rules, 1024 * 1024)
        if not isinstance(report, dict) or "config" not in report:
            raise ValueError("Input must be a Traffic Lab report")
        baseline, baseline_hash = read_json(args.baseline, 64 * 1024 * 1024) if args.baseline else (None, None)
        if args.baseline and (not isinstance(baseline, dict) or "config" not in baseline):
            raise ValueError("Baseline input must be a complete Traffic Lab report")
        result = evaluate(report, rules, baseline)
        if baseline_hash is not None:
            result["baseline_sha256"] = baseline_hash
        result.update(source_sha256=source_hash, rules_sha256=rules_hash,
                      validator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
        args.output.mkdir(parents=True, exist_ok=False)
        write(result, args.output)
    except (OSError, ValueError, KeyError, TypeError, OverflowError, RecursionError) as error:
        print(json.dumps({"status": "invalid_input", "error": str(error)}), file=sys.stderr)
        return 2
    print(json.dumps({"status": "passed" if result["passed"] else "failed", "failed_assertions": result["failed_assertions"], "report": str(args.output / "validation.json")}))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
