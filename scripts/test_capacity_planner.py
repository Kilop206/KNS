from contextlib import redirect_stderr, redirect_stdout
import copy
import io
import json
import os
from pathlib import Path
import tempfile
import unittest

import capacity_planner


class CapacityPlannerTests(unittest.TestCase):
    def test_growth_rounding_bounds_and_pareto_are_explicit(self):
        self.assertEqual(capacity_planner.populations(100, [0, 10, 25, 50, 100]), [100, 110, 125, 150, 200])
        with self.assertRaises(ValueError): capacity_planner.populations(100, [1000])
        with self.assertRaises(ValueError): capacity_planner.populations(10, [1, 2])
        base = {"sensor_count": 10, "user_cost": 1, "mean_network_data_latency_s": 0.01,
                "delivery_ratio": 1, "sensor_on_time_ratio": 1}
        rows = [copy.deepcopy(base), {**base, "user_cost": 2, "mean_network_data_latency_s": 0.02},
                {**base, "sensor_count": 20, "user_cost": 3}, {**base, "user_cost": None}]
        # Missing prices disable the cost frontier for the actual CLI matrix;
        # this unit fixture tests the explicit unavailable marker independently.
        priced = rows[:3]; capacity_planner.pareto(priced)
        self.assertEqual([row["pareto_efficient"] for row in priced], [True, False, True])
        capacity_planner.pareto(rows[-1:]); self.assertIsNone(rows[-1]["pareto_efficient"])
        capacity_planner.pareto(rows)
        self.assertTrue(all(row["pareto_efficient"] is None for row in rows))

    def test_invalid_matrix_or_costs_create_no_results(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); output = root / "capacity"
            for options in (["--max-cases", "1"], ["--growth-percent", "1000"], ["--bandwidth-mbps", "0"]):
                with redirect_stderr(io.StringIO()):
                    self.assertEqual(capacity_planner.main(["--exe", str(root / "missing.exe"), "--output", str(output), *options]), 1)
                self.assertFalse(output.exists())
            path = root / "costs.json"
            path.write_text(json.dumps({"schema_version": "1.0", "currency": "XXX", "uplinks": [{"bandwidth_mbps": 1, "cost": 10}]}))
            with self.assertRaisesRegex(ValueError, "cover exactly"):
                capacity_planner.costs_from(path, [1, 2])

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for measured capacity alternatives")
    def test_actual_growth_and_bandwidth_alternatives_preserve_raw_evidence(self):
        with tempfile.TemporaryDirectory(prefix="KNS capacity ") as folder:
            root = Path(folder); output = root / "planner"
            costs = root / "user-test-costs.json"
            # Explicit arbitrary test units, not infrastructure market prices.
            costs.write_text(json.dumps({"schema_version": "1.0", "currency": "XXX", "sensor_unit_cost": 3,
                "uplinks": [{"bandwidth_mbps": 0.1, "cost": 1}, {"bandwidth_mbps": 2, "cost": 2}]}))
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(capacity_planner.main(["--exe", os.environ["KNS_TEST_EXE"], "--output", str(output),
                    "--base-sensors", "10", "--growth-percent", "0", "100", "--bandwidth-mbps", "0.1", "2", "--costs", str(costs)]), 0)
            report = json.loads((output / "capacity.json").read_text())
            self.assertEqual(report["case_count"], 4)
            self.assertEqual(report["frontier_status"], "user_cost_frontier")
            for row in report["results"]:
                self.assertTrue(Path(row["manifest"]).exists())
                raw = json.loads(Path(row["traffic_report"]).read_text())
                self.assertEqual(row["generated"], raw["generated"])
                self.assertEqual(row["delivery_ratio"], raw["delivered"] / raw["generated"])
                self.assertEqual(row["user_cost"], row["sensor_count"] * 3 + (1 if row["bandwidth_mbps"] == 0.1 else 2))
                if row["bandwidth_mbps"] == 2: self.assertTrue(row["meets_requirements"])
            slow = [row for row in report["results"] if row["bandwidth_mbps"] == 0.1]
            self.assertTrue(any(not row["meets_requirements"] for row in slow))
            with redirect_stderr(io.StringIO()):
                self.assertEqual(capacity_planner.main(["--exe", os.environ["KNS_TEST_EXE"], "--output", str(output)]), 1)
            self.assertEqual(json.loads((output / "capacity.json").read_text()), report)
            resilient = root / "resilient"
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(capacity_planner.main(["--exe", os.environ["KNS_TEST_EXE"], "--output", str(resilient),
                    "--base-sensors", "10", "--growth-percent", "0", "--bandwidth-mbps", "2", "--redundant", "--with-outage"]), 0)
            unpriced = json.loads((resilient / "capacity.json").read_text())
            self.assertEqual(unpriced["frontier_status"], "no_user_costs")
            self.assertTrue(unpriced["results"][0]["meets_requirements"])
            self.assertIsNone(unpriced["results"][0]["user_cost"])
            self.assertIsNone(unpriced["results"][0]["pareto_efficient"])


if __name__ == "__main__":
    unittest.main()
