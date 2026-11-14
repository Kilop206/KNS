from contextlib import redirect_stderr
import io
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

import validate_result

ROOT = Path(__file__).resolve().parent.parent
LAB = ROOT / "KNS Experiments/traffic-lab"


class ResultValidationTests(unittest.TestCase):
    def test_invalid_rules_are_explicit_and_unavailable_metrics_do_not_pass(self):
        rule = {"metric": "delivery_ratio", "operator": ">=", "value": 0.9}
        for changes in ({"metric": "invented"}, {"operator": "approx"}, {"value": True},
                        {"value": float("nan")}, {"value": 10**1000}, {"flow_id": 0}, {"name": "<script>"}):
            with self.assertRaises(ValueError):
                validate_result.validate_rules({"schema_version": "1.0", "assertions": [{**rule, **changes}]})
        with self.assertRaisesRegex(ValueError, "require flow_id"):
            validate_result.validate_rules({"schema_version": "1.0", "assertions": [{**rule, "metric": "operation_latency_p95_s"}]})
        observed, unit, scope, evidence = validate_result.measure({"flows": [{"generated": 0, "delivered": 0}]}, rule)
        self.assertIsNone(observed); self.assertEqual(unit, "fraction"); self.assertEqual(evidence["denominator"], 0)

    def test_baseline_rule_versions_offsets_and_finite_thresholds(self):
        relative = {"metric": "delivery_ratio", "operator": ">=", "baseline_offset": -0.01}
        validate_result.validate_rules({"schema_version": "1.1", "assertions": [relative]})
        validate_result.validate_rules({"schema_version": "1.1", "assertions": [{"metric": "pending", "operator": "==", "value": 0}]})
        invalid = [("1.0", relative), ("2.0", relative), ("1.1", {**relative, "value": 0}),
            ("1.1", {"metric": "pending", "operator": "=="})]
        invalid.extend(("1.1", {**relative, "baseline_offset": value}) for value in (True, float("nan"), float("inf"), 10**1000))
        for version, rule in invalid:
            with self.subTest(version=version, rule=rule), self.assertRaises(ValueError):
                validate_result.validate_rules({"schema_version": version, "assertions": [rule]})
        self.assertAlmostEqual(validate_result.offset_threshold(0.99, -0.01), 0.98)
        self.assertEqual(validate_result.offset_threshold(1000, 2), 1002)
        with self.assertRaisesRegex(ValueError, "non-finite"):
            validate_result.offset_threshold(1e308, 1e308)

    def test_duplicate_json_fields_are_rejected_before_evaluation(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "duplicate.json"
            source.write_text('{"config":{"seed":42,"seed":43}}')
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                validate_result.read_json(source, 1024)
            with self.assertRaisesRegex(ValueError, "size limit"):
                validate_result.read_json(source, 1)

    def test_invalid_input_preserves_sources_and_existing_results(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "bad.json"; source.write_text('{"bad": true}')
            rules = root / "rules.json"; rules.write_text('{"schema_version":"1.0","assertions":[]}')
            output = root / "existing"; output.mkdir()
            sentinel = output / "keep.txt"; sentinel.write_text("existing evidence")
            with redirect_stderr(io.StringIO()):
                self.assertEqual(validate_result.main([str(source), "--rules", str(rules), "--output", str(output)]), 2)
            self.assertEqual(source.read_text(), '{"bad": true}')
            self.assertEqual(sentinel.read_text(), "existing evidence")

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for actual regression validation")
    def test_actual_bandwidth_regression_causes_process_failure_with_junit_diagnostics(self):
        with tempfile.TemporaryDirectory(prefix="KNS validation ") as folder:
            root = Path(folder)
            rules = root / "rules.json"
            rules.write_text(json.dumps({"schema_version": "1.0", "assertions": [
                {"name": "delivery-target", "metric": "delivery_ratio", "operator": ">=", "value": 0.9}]}))
            for bandwidth, expected_code in ((10, 0), (1, 1)):
                topology = json.loads((LAB / "topology.json").read_text())
                topology["links"][1]["bandwidth"] = bandwidth
                source = root / f"network {bandwidth}.json"; source.write_text(json.dumps(topology))
                output = root / f"run {bandwidth}.csv"
                result = subprocess.run([os.environ["KNS_TEST_EXE"], "--headless", "--topology", str(source),
                    "--traffic-plan", str(LAB / "traffic.json"), "--output", str(output)], capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)
                report = Path(str(output) + ".traffic.json"); original = report.read_bytes()
                target = root / f"validation {bandwidth}"
                checked = subprocess.run([sys.executable, str(ROOT / "scripts/validate_result.py"), str(report),
                    "--rules", str(rules), "--output", str(target)], capture_output=True, text=True, timeout=30)
                self.assertEqual(checked.returncode, expected_code, checked.stderr)
                verdict = json.loads((target / "validation.json").read_text())
                self.assertEqual(verdict["passed"], expected_code == 0)
                self.assertEqual(verdict["checks"][0]["scope"], "generated")
                junit = ET.parse(target / "validation.xml").getroot()
                self.assertEqual(int(junit.attrib["failures"]), expected_code)
                self.assertEqual(report.read_bytes(), original)
                repeat = subprocess.run([sys.executable, str(ROOT / "scripts/validate_result.py"), str(report),
                    "--rules", str(rules), "--output", str(target)], capture_output=True, timeout=30)
                self.assertEqual(repeat.returncode, 2)
                self.assertEqual(json.loads((target / "validation.json").read_text()), verdict)
                replicated = root / f"validation repeat {bandwidth}"
                repeated = subprocess.run([sys.executable, str(ROOT / "scripts/validate_result.py"), str(report),
                    "--rules", str(rules), "--output", str(replicated)], capture_output=True, timeout=30)
                self.assertEqual(repeated.returncode, expected_code)
                for name in ("validation.json", "validation.xml", "validation.md"):
                    self.assertEqual((target / name).read_bytes(), (replicated / name).read_bytes())
                if bandwidth == 10:
                    unavailable = {"schema_version": "1.0", "assertions": [
                        {"metric": "operation_completion_ratio", "operator": ">=", "value": 0}]}
                    result = validate_result.evaluate(json.loads(original), unavailable)
                    self.assertFalse(result["passed"])
                    self.assertIsNone(result["checks"][0]["observed"])

            baseline = root / "run 10.csv.traffic.json"
            baseline_rules = root / "baseline-rules.json"
            baseline_rules.write_text(json.dumps({"schema_version": "1.1", "assertions": [
                {"name": "delivery-baseline", "metric": "delivery_ratio", "operator": ">=", "baseline_offset": -0.01},
                {"name": "drained", "metric": "pending", "operator": "==", "value": 0}]}))
            for bandwidth, expected_code in ((10, 0), (1, 1)):
                source = root / f"run {bandwidth}.csv.traffic.json"
                output = root / f"baseline verdict {bandwidth}"
                checked = subprocess.run([sys.executable, str(ROOT / "scripts/validate_result.py"), str(source),
                    "--baseline", str(baseline), "--rules", str(baseline_rules), "--output", str(output)],
                    capture_output=True, text=True, timeout=60)
                self.assertEqual(checked.returncode, expected_code, checked.stderr)
                verdict = json.loads((output / "validation.json").read_text())
                self.assertEqual(verdict["schema_version"], "1.1")
                self.assertEqual(verdict["baseline_sha256"], hashlib.sha256(baseline.read_bytes()).hexdigest())
                self.assertEqual(verdict["baseline_configuration_sha256"], verdict["configuration_sha256"])
                check = verdict["checks"][0]
                self.assertAlmostEqual(check["expected"], check["baseline"]["observed"] - 0.01)
                self.assertEqual(check["baseline"]["scope"], "generated")
                junit = ET.parse(output / "validation.xml").getroot()
                self.assertEqual(int(junit.attrib["failures"]), expected_code)
                self.assertIn("baseline=", junit.find("testcase/system-out").text)
                self.assertIn("Baseline thresholds", (output / "validation.md").read_text())
            for args, name in (([], "missing"), (["--baseline", str(baseline), "--rules", str(rules)], "unused")):
                output = root / ("bad baseline " + name)
                arguments = [str(baseline), "--rules", str(baseline_rules), "--output", str(output)] + args
                with redirect_stderr(io.StringIO()):
                    self.assertEqual(validate_result.main(arguments), 2)
                self.assertFalse(output.exists())
            empty_rule = {"schema_version": "1.1", "assertions": [
                {"metric": "operation_completion_ratio", "operator": "!=", "baseline_offset": 0}]}
            report = json.loads(baseline.read_text())
            unavailable = validate_result.evaluate(report, empty_rule, report)
            self.assertFalse(unavailable["passed"])
            self.assertIsNone(unavailable["checks"][0]["expected"])
            self.assertIsNone(unavailable["checks"][0]["baseline"]["observed"])
            self.assertIn("baseline metric unavailable", unavailable["checks"][0]["reason"])
            absolute_rules = {"schema_version": "1.0", "assertions": [{"metric": "pending", "operator": "==", "value": 0}]}
            null_baseline = root / "null baseline.json"; null_baseline.write_text("null")
            null_rules = root / "absolute rules.json"; null_rules.write_text(json.dumps(absolute_rules))
            null_output = root / "invalid null baseline"
            with redirect_stderr(io.StringIO()):
                self.assertEqual(validate_result.main([str(baseline), "--baseline", str(null_baseline), "--rules", str(null_rules), "--output", str(null_output)]), 2)
            self.assertFalse(null_output.exists())
            for kind in ("model", "stream", "cohort"):
                tampered = json.loads(baseline.read_text())
                if kind == "model":
                    tampered["model"] = "different model"
                elif kind == "stream":
                    tampered["flows"][0]["stream_seed"] ^= 1
                else:
                    flow = tampered["flows"][0]
                    for key in ("generated", "delivered", "source_generated", "delivered_on_time"):
                        flow[key] += 1
                    flow["delivered_wire_bytes"] += flow["config"]["wire_bytes"]
                    tampered["generated"] += 1; tampered["delivered"] += 1
                validate_result.traffic_config.validate_report(tampered, tampered["config"])
                with self.subTest(tampering=kind), self.assertRaises(ValueError):
                    validate_result.evaluate(report, json.loads(baseline_rules.read_text()), tampered)
            plan = json.loads((LAB / "traffic.json").read_text()); plan["seed"] = 43
            changed_plan = root / "different seed.json"; changed_plan.write_text(json.dumps(plan))
            changed_output = root / "different seed.csv"
            result = subprocess.run([os.environ["KNS_TEST_EXE"], "--headless", "--seed", "43", "--topology", str(LAB / "topology.json"),
                "--traffic-plan", str(changed_plan), "--output", str(changed_output)], capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            changed = json.loads(Path(str(changed_output) + ".traffic.json").read_text())
            with self.assertRaisesRegex(ValueError, "configuration must match"):
                validate_result.evaluate(report, json.loads(baseline_rules.read_text()), changed)


if __name__ == "__main__":
    unittest.main()
