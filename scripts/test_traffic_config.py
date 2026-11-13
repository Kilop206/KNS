import copy
import csv
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

import traffic_config

ROOT = Path(__file__).resolve().parent.parent
LAB = ROOT / "KNS Experiments/traffic-lab"


class TrafficConfigTests(unittest.TestCase):
    def setUp(self):
        self.plan = json.loads((LAB / "traffic.json").read_text())
        self.topology = json.loads((LAB / "topology.json").read_text())

    def test_example_validates_every_schedule(self):
        traffic_config.validate(self.plan, self.topology)
        self.assertEqual(len({f["schedule"]["kind"] for f in self.plan["flows"]}), 6)

    def test_invalid_plans_have_explicit_preflight_errors(self):
        for mutation in (
            lambda p: p.update(seed=-1), lambda p: p.update(max_packets=True),
            lambda p: p["flows"].append(copy.deepcopy(p["flows"][0])),
            lambda p: p["flows"][0].update(destination=99),
            lambda p: p["flows"][0].update(end_s=0),
            lambda p: p["flows"][0].update(wire_bytes=0),
            lambda p: p["flows"][0]["schedule"].update(interval_s=float("nan")),
            lambda p: p["flows"][0]["schedule"].update(kind=[]),
            lambda p: p["flows"][3]["schedule"].update(packet_interval_s=1),
            lambda p: p["flows"][4]["schedule"].update(max_interval_s=0.01),
            lambda p: p["flows"][5]["schedule"].update(offsets_s=[0.03, 0.01]),
        ):
            invalid = copy.deepcopy(self.plan); mutation(invalid)
            with self.assertRaises(ValueError):
                traffic_config.validate(invalid, self.topology)

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for real engine integration")
    def test_actual_cli_competition_replay_conservation_and_invalid_arguments(self):
        with tempfile.TemporaryDirectory(prefix="KNS traffic ") as temp:
            root = Path(temp)
            def execute(output, *extra):
                return subprocess.run([os.environ["KNS_TEST_EXE"], "--headless", "--topology", str(LAB / "topology.json"),
                    "--traffic-plan", str(LAB / "traffic.json"), "--analytics", str(LAB / "analytics.json"),
                    "--seed", "42", "--output", str(output), *extra], capture_output=True, text=True, timeout=30)
            original = None
            for index in range(2):
                output = root / f"run {index}.csv"
                result = execute(output)
                self.assertEqual(result.returncode, 0, result.stderr)
                source = Path(str(output) + ".traffic.json")
                report = json.loads(source.read_text())
                traffic_config.validate_report(report, self.plan)
                self.assertGreater(report["generated"], 200)
                self.assertGreater(report["dropped"], 0)
                self.assertGreater(report["delivered"], 0)
                self.assertEqual(report["flows"][0]["delivered_on_time"] + report["flows"][0]["delivered_late"], report["flows"][0]["delivered"])
                if original is None: original = source.read_bytes()
                else: self.assertEqual(original, source.read_bytes())
                with output.open(newline="") as stream:
                    self.assertEqual(int(next(csv.DictReader(stream))["total_sessions"]), 0)
                analytics = json.loads(Path(str(output) + ".analytics.json").read_text())
                self.assertEqual(analytics["tcp_contiguous_payload_bytes"], 0)
                self.assertTrue(all(g["key"].startswith("traffic:") for g in analytics["flow_groups"]))
                for key in ("generated", "pending", "delivered"):
                    invalid = copy.deepcopy(report); invalid[key] += 1
                    with self.assertRaises(ValueError): traffic_config.validate_report(invalid, self.plan)
            partial = root / "partial.csv"
            self.assertEqual(execute(partial, "--until", "0.001").returncode, 3)
            observed = json.loads(Path(str(partial) + ".traffic.json").read_text())
            traffic_config.validate_report(observed, self.plan, complete=False)
            with self.assertRaises(ValueError): traffic_config.validate_report(observed, self.plan)
            self.assertEqual(execute(root / "duplicate.csv", "--traffic-plan", str(LAB / "traffic.json")).returncode, 1)
            bad = copy.deepcopy(self.plan); bad["flows"][0]["destination"] = 99
            source = root / "bad.json"; source.write_text(json.dumps(bad))
            result = subprocess.run([os.environ["KNS_TEST_EXE"], "--headless", "--topology", str(LAB / "topology.json"),
                                     "--traffic-plan", str(source)], capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 1)
            self.assertIn(b"Invalid traffic plan", result.stderr)
            limited = copy.deepcopy(self.plan); limited["max_packets"] = 2
            source.write_text(json.dumps(limited))
            output = root / "limited.csv"
            result = subprocess.run([os.environ["KNS_TEST_EXE"], "--headless", "--topology", str(LAB / "topology.json"),
                "--traffic-plan", str(source), "--output", str(output)], capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 3, result.stderr)
            report = json.loads(Path(str(output) + ".traffic.json").read_text())
            self.assertTrue(report["generation_limit_reached"])
            traffic_config.validate_report(report, limited, complete=False)
            with self.assertRaises(ValueError): traffic_config.validate_report(report, limited)
            down = copy.deepcopy(self.topology)
            for link in down["links"]: link["up"] = False
            topology = root / "all links down.json"; topology.write_text(json.dumps(down))
            output = root / "all dropped.csv"
            result = subprocess.run([os.environ["KNS_TEST_EXE"], "--headless", "--topology", str(topology),
                "--traffic-plan", str(LAB / "traffic.json"), "--output", str(output)], capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(Path(str(output) + ".traffic.json").read_text())
            traffic_config.validate_report(report, self.plan)
            self.assertEqual(report["generated"], report["dropped"])
            self.assertEqual(report["delivered"], 0)


if __name__ == "__main__":
    unittest.main()
