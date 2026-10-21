import copy
import csv
import io
import json
from pathlib import Path
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import patch

import benchmark_suite
import compare_experiments as comparison


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.profile = {
            "name": "link-comparison", "experiments": [{"name": "control"}, {"name": "variant"}],
            "routing_metrics": ["delay", "bandwidth"], "seeds": [42, 43],
            "packet_sizes": [1500, 3000], "congestion_controls": ["reno", "cubic"],
        }
        self.suite = {
            "schema_version": "1.0", "name": self.profile["name"],
            "topologies": ["/old/location/topologies/001-control.json",
                           "/old/location/topologies/002-variant.json"],
            **{field: self.profile[field] for field in comparison.DEFAULTS},
            "link_event_scenarios": [{"name": "baseline", "events": []}],
        }
        rows = []
        for index, case in enumerate(benchmark_suite.expand_cases(self.suite)):
            control = case["topology"] == self.suite["topologies"][0]
            # Different absolute values for each workload catch accidental cross-pairing.
            value = (1000 * case["seed"] + case["packet_size"]
                     + (100 if case["routing_metric"] == "bandwidth" else 0)
                     + (10 if case["congestion_control"] == "cubic" else 0))
            rows.append({
                **case, "case_id": f"case-{index}", "status": "success", "returncode": 0,
                "delivery_rate": 1 if control else 0.9,
                "loss_rate": 0 if control else 0.1,
                "throughput_pps": value if control else value - (5 if case["seed"] == 42 else 15),
                "avg_latency_s": value if control else value + 2,
                "simulation_duration_s": value if control else value + 3,
                "queue_overflow_drops": 10 if control else 4,
                "red_early_drops": 0 if control else 2,
            })
        self.benchmark = {"schema_version": "1.0", "suite": self.suite,
                          "kns_commit": "test-commit", "cases": rows}

    def test_pairs_match_every_workload_dimension_and_summarize_only_seeds(self):
        result = comparison.build_comparison(self.profile, self.benchmark)
        self.assertEqual(result["baseline"], "control")
        self.assertEqual(result["expected_pairs"], 16)
        self.assertEqual(result["matched_pairs"], 16)
        self.assertEqual(len(result["summaries"]), 8)
        for summary in result["summaries"]:
            self.assertEqual(summary["expected_pairs"], 2)
            self.assertEqual(summary["metrics"]["throughput_pps"],
                             {"sample_count": 2, "mean_delta": -10, "min_delta": -15, "max_delta": -5})
            self.assertEqual(summary["metrics"]["avg_latency_s"]["mean_delta"], 2)
            self.assertEqual(summary["metrics"]["queue_overflow_drops"]["mean_delta"], -6)
            self.assertAlmostEqual(summary["metrics"]["delivery_rate"]["mean_delta"], -0.1)

    def test_reference_can_be_selected_in_profile_or_overridden(self):
        self.profile["baseline"] = "variant"
        result = comparison.build_comparison(self.profile, self.benchmark)
        self.assertEqual(result["pairs"][0]["experiment"], "control")
        self.assertEqual(result["pairs"][0]["avg_latency_s_delta"], -2)
        result = comparison.build_comparison(self.profile, self.benchmark, baseline="control")
        self.assertEqual(result["pairs"][0]["avg_latency_s_delta"], 2)
        with self.assertRaises(ValueError):
            comparison.build_comparison(self.profile, self.benchmark, baseline="missing")

    def test_failed_transport_stats_and_missing_cases_remain_unavailable(self):
        baseline_rows = self.benchmark["cases"][:16]
        variant_rows = self.benchmark["cases"][16:]
        baseline_rows[0].update(status="transport_failure", returncode=2)
        variant_rows[1].update(status="stats_error", returncode=0)
        self.benchmark["cases"].remove(variant_rows[2])
        result = comparison.build_comparison(self.profile, self.benchmark)
        self.assertEqual(result["matched_pairs"], 13)
        self.assertEqual(result["unavailable_pairs"], 3)
        self.assertEqual(result["pairs"][0]["baseline_status"], "transport_failure")
        self.assertEqual(result["pairs"][1]["experiment_status"], "stats_error")
        self.assertEqual(result["pairs"][2]["experiment_status"], "missing")
        for pair in result["pairs"][:3]:
            self.assertIsNone(pair["avg_latency_s_delta"])

    def test_undefined_metric_is_counted_per_metric_without_filling_zero(self):
        self.benchmark["cases"][16]["throughput_pps"] = None
        result = comparison.build_comparison(self.profile, self.benchmark)
        metric = result["summaries"][0]["metrics"]["throughput_pps"]
        self.assertEqual(metric["sample_count"], 1)
        self.assertEqual(metric["mean_delta"], -15)
        self.assertEqual(result["summaries"][0]["metrics"]["avg_latency_s"]["sample_count"], 2)
        self.assertEqual(result["matched_pairs"], 16)

    def test_all_failed_results_report_no_numeric_aggregate(self):
        for row in self.benchmark["cases"]:
            row.update(status="timeout", returncode=124)
        result = comparison.build_comparison(self.profile, self.benchmark)
        self.assertEqual(result["matched_pairs"], 0)
        for summary in result["summaries"]:
            self.assertEqual(summary["metrics"]["avg_latency_s"]["sample_count"], 0)
            self.assertIsNone(summary["metrics"]["avg_latency_s"]["mean_delta"])

    def test_inconsistent_artifacts_are_rejected(self):
        mutations = [
            lambda data: data.update(schema_version="2.0"),
            lambda data: data["suite"].update(name="other"),
            lambda data: data["suite"].update(seeds=[99]),
            lambda data: data["suite"]["topologies"].reverse(),
            lambda data: data["cases"].append(copy.deepcopy(data["cases"][0])),
            lambda data: data["cases"][0].update(seed=99),
            lambda data: data["cases"][0].update(link_events=["1:0:1:down"]),
            lambda data: data["cases"][0].update(avg_latency_s=float("nan")),
            lambda data: data["cases"][0].update(avg_latency_s=True),
        ]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                benchmark = copy.deepcopy(self.benchmark)
                mutation(benchmark)
                with self.assertRaises(ValueError):
                    comparison.build_comparison(self.profile, benchmark)

    def test_single_experiment_produces_an_empty_comparison(self):
        self.profile["experiments"] = self.profile["experiments"][:1]
        self.suite["topologies"] = self.suite["topologies"][:1]
        self.benchmark["cases"] = self.benchmark["cases"][:16]
        result = comparison.build_comparison(self.profile, self.benchmark)
        self.assertEqual(result["expected_pairs"], 0)
        self.assertEqual(result["summaries"], [])

    def test_duplicate_seeds_and_boolean_workload_settings_are_rejected(self):
        self.profile["seeds"] = [42, 42]
        self.suite["seeds"] = [42, 42]
        with self.assertRaises(ValueError):
            comparison.build_comparison(self.profile, self.benchmark)
        self.profile["seeds"] = [42, 43]
        self.suite["seeds"] = [42, 43]
        self.benchmark["cases"][0]["seed"] = 42.0
        with self.assertRaises(ValueError):
            comparison.build_comparison(self.profile, self.benchmark)

    def test_offline_cli_uses_snapshots_and_preserves_raw_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / "runs").mkdir()
            (directory / "profile.json").write_text(json.dumps(self.profile), encoding="utf-8")
            benchmark_path = directory / "runs" / "benchmark.json"
            benchmark_path.write_text(json.dumps(self.benchmark), encoding="utf-8")
            original = benchmark_path.read_bytes()
            with patch.object(benchmark_suite.subprocess, "run", side_effect=AssertionError("Must not run KNS")), \
                    redirect_stdout(io.StringIO()):
                self.assertEqual(comparison.main([str(directory)]), 0)
                self.assertEqual(comparison.main([str(directory), "--baseline", "variant"]), 0)
            self.assertEqual(benchmark_path.read_bytes(), original)
            result = json.loads((directory / "comparison.json").read_text())
            self.assertEqual(result["baseline"], "variant")
            with (directory / "comparison.csv").open(newline="") as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 16)
            markdown = (directory / "comparison.md").read_text()
            self.assertIn("16/16", markdown)
            self.assertIn("experiment minus baseline", markdown)

    def test_invalid_cli_inputs_leave_existing_comparisons_untouched(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            report = directory / "comparison.json"
            report.write_text("preserve previous comparison")
            with redirect_stderr(io.StringIO()):
                self.assertEqual(comparison.main([str(directory)]), 1)
            self.assertEqual(report.read_text(), "preserve previous comparison")


if __name__ == "__main__":
    unittest.main()
