import copy
import csv
import io
import json
import math
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
            self.assertEqual({key: summary["metrics"]["throughput_pps"][key]
                              for key in ("sample_count", "mean_delta", "min_delta", "max_delta")},
                             {"sample_count": 2, "mean_delta": -10, "min_delta": -15, "max_delta": -5})
            self.assertEqual(summary["metrics"]["avg_latency_s"]["mean_delta"], 2)
            self.assertEqual(summary["metrics"]["queue_overflow_drops"]["mean_delta"], -6)
            self.assertAlmostEqual(summary["metrics"]["delivery_rate"]["mean_delta"], -0.1)

    def test_variability_and_conditional_interval_match_known_paired_solution(self):
        result = comparison.build_comparison(self.profile, self.benchmark)
        metric = result["summaries"][0]["metrics"]["throughput_pps"]
        self.assertEqual(metric["median_delta"], -10)
        self.assertAlmostEqual(metric["sample_stddev_delta"], math.sqrt(50))
        self.assertAlmostEqual(metric["conditional_ci95"]["low"], -73.53)
        self.assertAlmostEqual(metric["conditional_ci95"]["high"], 53.53)
        self.assertEqual(metric["direction"], "regression")
        self.assertEqual(metric["sample_count"], 2)
        # Constant deltas are descriptive evidence, not an assertion of zero uncertainty.
        constant = result["summaries"][0]["metrics"]["avg_latency_s"]
        self.assertIsNone(constant["conditional_ci95"])
        self.assertEqual(constant["ci_status"], "no_observed_variation")

    def test_relative_deltas_zero_baselines_and_context_metrics(self):
        result = comparison.build_comparison(self.profile, self.benchmark)
        pair = result["pairs"][0]
        self.assertAlmostEqual(pair["delivery_rate_delta_percent"], -10)
        self.assertEqual(pair["delivery_rate_baseline"], 1)
        self.assertEqual(pair["delivery_rate_experiment"], 0.9)
        self.assertIsNone(pair["loss_rate_delta_percent"])
        self.assertEqual(pair["loss_rate_direction"], "regression")
        self.assertEqual(pair["simulation_duration_s_direction"], "context_dependent")
        self.assertEqual(pair["red_early_drops_direction"], "context_dependent")
        self.assertEqual(result["summaries"][0]["metrics"]["loss_rate"]["relative_sample_count"], 0)

    def test_relative_summary_averages_pairs_instead_of_dividing_group_means(self):
        for row in self.benchmark["cases"]:
            reference = 1 if row["seed"] == 42 else 3
            row["throughput_pps"] = reference if row["topology"] == self.suite["topologies"][0] else reference + 1
        result = comparison.build_comparison(self.profile, self.benchmark)
        metric = result["summaries"][0]["metrics"]["throughput_pps"]
        self.assertEqual(metric["baseline_mean"], 2)
        self.assertEqual(metric["experiment_mean"], 3)
        self.assertAlmostEqual(metric["mean_delta_percent"], 200 / 3)

    def test_single_pair_has_no_interval_and_matrix_is_bounded_before_expansion(self):
        self.benchmark["cases"].pop(16)
        result = comparison.build_comparison(self.profile, self.benchmark)
        metric = result["summaries"][0]["metrics"]["avg_latency_s"]
        self.assertEqual(metric["sample_count"], 1)
        self.assertIsNone(metric["sample_stddev_delta"])
        self.assertIsNone(metric["conditional_ci95"])
        self.assertEqual(metric["ci_status"], "insufficient_samples")
        self.profile["seeds"] = list(range(20000))
        self.suite["seeds"] = self.profile["seeds"]
        with patch.object(comparison.itertools, "product", side_effect=AssertionError("must preflight")):
            with self.assertRaisesRegex(ValueError, "10000-case budget"):
                comparison.build_comparison(self.profile, self.benchmark)

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
            self.assertTrue((directory / "comparison.html").is_file())
            self.assertTrue((directory / "comparison-summary.csv").is_file())

    def test_html_escapes_artifact_text_and_contains_only_local_visuals(self):
        self.profile["name"] = '<script>alert(1)</script>'
        self.suite["name"] = self.profile["name"]
        result = comparison.build_comparison(self.profile, self.benchmark)
        with tempfile.TemporaryDirectory() as temporary:
            comparison.write_reports(result, Path(temporary))
            html = (Path(temporary) / "comparison.html").read_text(encoding="utf-8")
            self.assertNotIn("<script>", html)
            self.assertIn("&lt;script&gt;", html)
            self.assertIn("Content-Security-Policy", html)
            self.assertIn('<svg xmlns=', html)
            self.assertNotIn('src="http', html)
            self.assertNotIn('<link ', html)
            self.assertEqual(html.count('<svg xmlns='), len(result["summaries"]) * len(comparison.METRICS))
            with (Path(temporary) / "comparison-summary.csv").open(newline="") as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 56)

    def test_cross_bundle_pairing_route_mapping_and_preserved_sources(self):
        import comparison_artifacts
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ("left", "right"):
                directory = root / name
                (directory / "runs").mkdir(parents=True)
                (directory / "profile.json").write_text(json.dumps(self.profile))
                (directory / "runs/benchmark.json").write_text(json.dumps(self.benchmark))
            output = root / "comparison"
            original = (root / "right/runs/benchmark.json").read_bytes()
            with redirect_stdout(io.StringIO()):
                self.assertEqual(comparison.main([str(root / "left"), "--against", str(root / "right"),
                    "--baseline", "control", "--candidate", "variant", "--reference-routing", "delay",
                    "--candidate-routing", "bandwidth", "--output", str(output)]), 0)
            result = json.loads((output / "comparison.json").read_text())
            self.assertEqual(result["matched_pairs"], 8)
            self.assertEqual(result["pairs"][0]["avg_latency_s_delta"], 102)
            self.assertEqual(result["pairs"][0]["routing_metric"], "delay vs bandwidth")
            self.assertEqual(result["pairs"][0]["experiment_routing_metric"], "bandwidth")
            self.assertEqual(result["sources"]["candidate"]["experiment"], "variant")
            self.assertEqual((root / "right/runs/benchmark.json").read_bytes(), original)
            with redirect_stderr(io.StringIO()):
                self.assertEqual(comparison.main([str(root / "left"), "--against", str(root / "right")]), 1)
            with self.assertRaises(ValueError):
                comparison_artifacts.build(root / "left", root / "right", reference_routing="delay")
            with redirect_stderr(io.StringIO()):
                self.assertEqual(comparison.main([str(root / "left"), "--against", str(root / "right"),
                    "--output", str(output)]), 1)
            self.assertEqual((root / "right/runs/benchmark.json").read_bytes(), original)

    def test_cross_bundle_different_seeds_stay_unavailable(self):
        import comparison_artifacts
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name, seed_offset in (("left", 0), ("right", 100)):
                directory = root / name
                (directory / "runs").mkdir(parents=True)
                profile = copy.deepcopy(self.profile)
                benchmark = copy.deepcopy(self.benchmark)
                profile["seeds"] = [seed + seed_offset for seed in profile["seeds"]]
                benchmark["suite"]["seeds"] = profile["seeds"]
                for row in benchmark["cases"]:
                    row["seed"] += seed_offset
                (directory / "profile.json").write_text(json.dumps(profile))
                (directory / "runs/benchmark.json").write_text(json.dumps(benchmark))
            result = comparison_artifacts.build(root / "left", root / "right")
            self.assertEqual(result["matched_pairs"], 0)
            self.assertEqual(result["unavailable_pairs"], 32)
            self.assertTrue(all(summary["metrics"]["throughput_pps"]["mean_delta"] is None for summary in result["summaries"]))

    def test_cross_bundle_records_different_abstract_workload_inputs(self):
        import comparison_artifacts
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name, rate in (("left", 1000), ("right", 2000)):
                directory = root / name; (directory / "runs").mkdir(parents=True)
                profile = copy.deepcopy(self.profile)
                profile["traffic_plan"] = {"schema_version": "1.0", "seed": 42, "max_packets": 100,
                    "flows": [{"id": 1, "source": 0, "destination": 1, "start_s": 0, "end_s": 1,
                               "wire_bytes": 125, "schedule": {"kind": "cbr", "rate_bps": rate}}]}
                (directory / "profile.json").write_text(json.dumps(profile))
                (directory / "runs/benchmark.json").write_text(json.dumps(self.benchmark))
            result = comparison_artifacts.build(root / "left", root / "right")
            self.assertFalse(result["methodology"]["traffic_plan_inputs_match"])
            self.assertEqual(result["sources"]["reference"]["workload_model"], "abstract_wire")
            self.assertNotEqual(result["sources"]["reference"]["traffic_plan_sha256"], result["sources"]["candidate"]["traffic_plan_sha256"])

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
