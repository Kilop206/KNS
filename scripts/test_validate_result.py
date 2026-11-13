from contextlib import redirect_stderr
import io
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


if __name__ == "__main__":
    unittest.main()
