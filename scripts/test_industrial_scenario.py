from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import tempfile
import unittest

import industrial_scenario


class IndustrialScenarioTests(unittest.TestCase):
    def test_explicit_sensors_and_stable_primary_fault_target(self):
        topology, profile = industrial_scenario.configuration(100, True)
        self.assertEqual(sum(node["type"] == "iot" for node in topology["nodes"]), 102)
        self.assertEqual(len(profile["traffic_plan"]["flows"]), 101)
        self.assertEqual(topology["links"][0]["from"], 100)
        self.assertEqual(topology["links"][0]["to"], 101)
        self.assertEqual(len(topology["links"]), 105)
        self.assertEqual((topology, profile), industrial_scenario.configuration(100, True))

    def test_invalid_scale_and_existing_outputs_are_preserved(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "scenario"
            for count in (0, 1000, 10000):
                with redirect_stderr(io.StringIO()):
                    self.assertEqual(industrial_scenario.main(["--sensors", str(count), "--output", str(output)]), 1)
                self.assertFalse(output.exists())
            output.mkdir(); (output / "keep.txt").write_text("existing scenario")
            with redirect_stderr(io.StringIO()):
                self.assertEqual(industrial_scenario.main(["--output", str(output)]), 1)
            self.assertEqual((output / "keep.txt").read_text(), "existing scenario")

    def test_invalid_case_timeout_fails_before_creating_outputs(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "scenario"
            for value in ("0", "-1", "nan", "inf"):
                with self.subTest(timeout=value), redirect_stderr(io.StringIO()):
                    self.assertEqual(industrial_scenario.main(["--output", str(output), "--timeout", value]), 1)
                self.assertFalse(output.exists())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for real industrial scenarios")
    def test_100_sensor_uplink_failure_and_redundancy_use_real_routing(self):
        with tempfile.TemporaryDirectory(prefix="KNS industrial ") as folder:
            measurements = []
            for redundant in (False, True):
                output = Path(folder) / ("redundant" if redundant else "single")
                arguments = ["--sensors", "100", "--output", str(output), "--exe", os.environ["KNS_TEST_EXE"], "--timeout", "180"]
                if redundant: arguments.append("--redundant")
                stdout, stderr = io.StringIO(), io.StringIO()
                with redirect_stdout(stdout), redirect_stderr(stderr):
                    result = industrial_scenario.main(arguments)
                self.assertEqual(result, 0, stderr.getvalue() + stdout.getvalue())
                report = json.loads((output / "measurements.json").read_text())
                manifest = json.loads((output / "experiment/manifest.json").read_text())
                self.assertEqual(manifest["configuration"]["timeout_seconds"], 180)
                self.assertEqual(len(report["cases"]), 3)
                self.assertTrue(all(case["generated"] == 1010 for case in report["cases"]))
                self.assertTrue(all(case["control_operations_completed"] == 5 for case in report["cases"]))
                self.assertTrue(all(case["wall_clock_duration_s"] > 0 for case in report["cases"]))
                control, outage, congestion = report["cases"]
                if not redundant:
                    self.assertGreater(control["delivered"], outage["delivered"])
                self.assertGreater(congestion["dropped"], control["dropped"])
                benchmark = json.loads((output / "experiment/runs/benchmark.json").read_text())
                faults = json.loads(Path(benchmark["cases"][1]["fault_report"]).read_text())
                during = next(sample for sample in faults["observations"] if sample["at"] == 0.31)["probes"][0]
                recovered = next(sample for sample in faults["observations"] if sample["at"] == 0.51)["probes"][0]
                self.assertEqual(during["status"] == "reachable", redundant)
                if redundant: self.assertEqual(len(during["hops"]), 3)
                self.assertEqual(recovered["status"], "reachable")
                self.assertEqual(len(recovered["hops"]), 2)
                measurements.append(report)
            self.assertGreater(measurements[1]["cases"][1]["delivered"], measurements[0]["cases"][1]["delivered"])


if __name__ == "__main__":
    unittest.main()
