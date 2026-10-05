import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

import benchmark_suite
import run as runner


class StatsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "stats.csv"
        self.row = dict(packets_sent=10, packets_delivered=8, packets_lost=2,
                        total_latency=4, avg_latency=0.5, packets_in_transit=0,
                        total_sessions=1, data_packets_delivered=8,
                        schema_version=1, simulation_duration_s=2, seed=42)

    def write(self):
        with self.path.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=self.row)
            writer.writeheader()
            writer.writerow(self.row)

    def test_throughput_uses_logical_time(self):
        self.write()
        stats = runner.parse_stats(self.path)
        self.assertEqual(stats["delivery_rate"], 0.8)
        self.assertEqual(stats["loss_rate"], 0.2)
        for duration in (0.01, 100):
            result = runner.compute_stats(stats, duration)
            self.assertEqual(result["throughput_pps"], 4)
            self.assertEqual(result["wall_clock_duration_s"], duration)

    def test_empty_run(self):
        self.row.update(packets_sent=0, packets_delivered=0, packets_lost=0,
                        simulation_duration_s=0)
        self.write()
        stats = runner.parse_stats(self.path)
        self.assertEqual(stats["delivery_rate"], 0)
        self.assertIsNone(runner.compute_stats(stats, 1)["throughput_pps"])

    def test_invalid_schema(self):
        for key, value in (("schema_version", 2), ("avg_latency", "nan"),
                           ("packets_sent", -1), ("seed", "")):
            with self.subTest(key=key):
                original = self.row[key]
                self.row[key] = value
                self.write()
                with self.assertRaises(ValueError):
                    runner.parse_stats(self.path)
                self.row[key] = original
        self.path.unlink()
        with self.assertRaises(ValueError):
            runner.parse_stats(self.path)

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for engine integration")
    def test_build_command_carries_experiment_parameters(self):
        command = runner.build_command(
            Path("KNS"),
            Path("mesh4.json"),
            Path("stats.csv"),
            "hop-count",
            123,
            4096,
        )
        self.assertEqual(
            command[-8:],
            ["--routing-metric", "hop-count", "--seed", "123", "--packet-size", "4096",
             "--congestion-control", "reno"],
        )
        with_faults = runner.build_command(
            Path("KNS"),
            Path("mesh4.json"),
            Path("stats.csv"),
            link_events=["0.5:0:1:down", "2.5:0:1:up"],
        )
        self.assertEqual(
            with_faults[-4:],
            ["--link-event", "0.5:0:1:down", "--link-event", "2.5:0:1:up"],
        )

    def test_engine_csv(self):
        root = Path(__file__).resolve().parent.parent
        subprocess.run(runner.build_command(Path(os.environ["KNS_TEST_EXE"]),
                       root / "app/topologies/mesh4.json", self.path),
                       env={**os.environ, "KNS_AUTO_START": "1"}, check=True, timeout=30)
        stats = runner.parse_stats(self.path)
        self.assertGreater(stats["packets_delivered"], 0)
        self.assertGreater(stats["simulation_duration_s"], 0)


class BenchmarkSuiteTests(unittest.TestCase):
    def test_default_routing_baseline_is_valid_and_has_36_cases(self):
        suite = benchmark_suite.load_suite(benchmark_suite.DEFAULT_SUITE)
        cases = benchmark_suite.expand_cases(suite)
        self.assertEqual(suite["name"], "routing-baseline-v1")
        self.assertEqual(len(cases), 36)
        self.assertEqual(
            {case["routing_metric"] for case in cases},
            {"delay", "bandwidth", "hop-count", "delay-bandwidth"},
        )
        self.assertEqual({case["seed"] for case in cases}, {42, 43, 44})
        self.assertEqual({case["congestion_control"] for case in cases}, {"reno"})

    def test_tcp_congestion_baseline_expands_all_algorithms(self):
        root = Path(__file__).resolve().parent.parent
        suite = benchmark_suite.load_suite(root / "benchmarks/v1/tcp-congestion-baseline.json")
        cases = benchmark_suite.expand_cases(suite)
        self.assertEqual(suite["name"], "tcp-congestion-baseline-v1")
        self.assertEqual(len(cases), 24)
        self.assertEqual(
            {case["congestion_control"] for case in cases},
            {"tahoe", "reno", "newreno", "cubic"},
        )
        self.assertEqual({case["fault_scenario"] for case in cases}, {"baseline"})

    def test_aqm_baseline_pairs_only_queue_policy_on_same_bottleneck(self):
        root = Path(__file__).resolve().parent.parent
        suite = benchmark_suite.load_suite(root / "benchmarks/v1/aqm-baseline.json")
        cases = benchmark_suite.expand_cases(suite)
        self.assertEqual(suite["name"], "aqm-baseline-v1")
        self.assertEqual(len(cases), 10)
        self.assertEqual({case["seed"] for case in cases}, {42, 43, 44, 45, 46})
        self.assertEqual({case["congestion_control"] for case in cases}, {"reno"})

        import json
        drop = json.loads(
            (root / "benchmarks/v1/topologies/aqm-bottleneck-drop-tail.json").read_text()
        )
        red = json.loads(
            (root / "benchmarks/v1/topologies/aqm-bottleneck-red.json").read_text()
        )
        self.assertEqual(drop["nodes"], red["nodes"])
        self.assertEqual(len(drop["links"]), len(red["links"]))
        for index, (control, candidate) in enumerate(zip(drop["links"], red["links"])):
            if index != 1:
                self.assertEqual(control, candidate)
                continue
            self.assertEqual(control["queue_policy"], "drop_tail")
            self.assertEqual(candidate["queue_policy"], "red")
            stripped = dict(candidate)
            for key in (
                "queue_policy",
                "red_min_threshold",
                "red_max_threshold",
                "red_max_drop_probability",
            ):
                stripped.pop(key, None)
            baseline = dict(control)
            baseline.pop("queue_policy", None)
            self.assertEqual(baseline, stripped)
            self.assertEqual(candidate["red_min_threshold"], 2)
            self.assertEqual(candidate["red_max_threshold"], 6)
            self.assertEqual(candidate["red_max_drop_probability"], 0.25)

    def test_resilience_baseline_expands_control_and_outage(self):
        root = Path(__file__).resolve().parent.parent
        suite = benchmark_suite.load_suite(root / "benchmarks/v1/resilience-baseline.json")
        cases = benchmark_suite.expand_cases(suite)
        self.assertEqual(suite["name"], "resilience-baseline-v1")
        self.assertEqual(len(cases), 6)
        self.assertEqual(
            {case["fault_scenario"] for case in cases},
            {"baseline", "link01-outage"},
        )
        outage = next(case for case in cases if case["fault_scenario"] == "link01-outage")
        self.assertEqual(
            outage["link_events"],
            ["0.5:0:1:down", "2.5:0:1:up"],
        )

    def test_fault_comparison_uses_matching_baseline_and_fault_minus_control(self):
        common = {
            "topology": "app/topologies/mesh4.json",
            "routing_metric": "delay",
            "seed": 42,
            "packet_size": 1500,
            "congestion_control": "reno",
            "returncode": 0,
        }
        baseline = {
            **common,
            "case_id": "baseline",
            "fault_scenario": "baseline",
            "delivery_rate": 1.0,
            "loss_rate": 0.0,
            "throughput_pps": 10.0,
            "avg_latency_s": 0.10,
            "simulation_duration_s": 5.0,
        }
        fault = {
            **common,
            "case_id": "fault",
            "fault_scenario": "link01-outage",
            "delivery_rate": 0.9,
            "loss_rate": 0.1,
            "throughput_pps": 8.0,
            "avg_latency_s": 0.15,
            "simulation_duration_s": 6.0,
        }
        comparison = benchmark_suite.compare_fault_scenarios([baseline, fault])
        self.assertEqual(len(comparison), 1)
        row = comparison[0]
        self.assertAlmostEqual(row["delivery_rate_delta"], -0.1)
        self.assertAlmostEqual(row["loss_rate_delta"], 0.1)
        self.assertAlmostEqual(row["throughput_pps_delta"], -2.0)
        self.assertAlmostEqual(row["avg_latency_s_delta"], 0.05)
        self.assertAlmostEqual(row["simulation_duration_s_delta"], 1.0)




    def test_markdown_report_summarizes_cases_and_fault_deltas(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            metadata = {
                "suite": {"name": "resilience-baseline-v1"},
                "kns_version": "1.1.0",
                "kns_commit": "abc123",
                "generated_at": "2026-10-05T12:00:00+00:00",
                "platform": "test",
                "case_count": 2,
                "successful_cases": 2,
                "failed_cases": 0,
                "cases": [
                    {
                        "case_id": "baseline",
                        "status": "success",
                        "returncode": 0,
                        "delivery_rate": 1.0,
                        "loss_rate": 0.0,
                        "throughput_pps": 10.0,
                        "avg_latency_s": 0.1,
                        "simulation_duration_s": 5.0,
                    },
                    {
                        "case_id": "fault",
                        "status": "success",
                        "returncode": 0,
                        "delivery_rate": 0.9,
                        "loss_rate": 0.1,
                        "throughput_pps": 8.0,
                        "avg_latency_s": 0.15,
                        "simulation_duration_s": 6.0,
                    },
                ],
            }
            comparisons = [{
                "fault_scenario": "link01-outage",
                "seed": 42,
                "delivery_rate_delta": -0.1,
                "loss_rate_delta": 0.1,
                "throughput_pps_delta": -2.0,
                "avg_latency_s_delta": 0.05,
                "simulation_duration_s_delta": 1.0,
            }]
            benchmark_suite.write_markdown_report(metadata, root, comparisons)
            report = (root / "report.md").read_text(encoding="utf-8")
            self.assertIn("# resilience-baseline-v1", report)
            self.assertIn("Fault vs baseline", report)
            self.assertIn("link01-outage", report)
            self.assertIn("-2.0000", report)

class ProcessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def launch(self, code, name):
        topo = self.root / f"{name}.json"
        log = self.root / f"{name}.log"
        output = self.root / f"{name}.csv"
        with patch.object(runner, "build_command", return_value=[sys.executable, "-c", code]):
            proc, started, handle = runner.run_silent(Path(sys.executable), topo, log, output)
        def cleanup():
            if proc.poll() is None:
                runner._terminate_tree(proc)
            handle.close()
        self.addCleanup(cleanup)
        return proc, started, topo, log, output, handle

    def test_launch_deadline_and_later_completion(self):
        hung = self.launch("import time; time.sleep(60)", "hung")
        done = self.launch("raise SystemExit(3)", "done")
        done[0].wait(timeout=5)
        pending, records = [hung, done], []
        runner._flush_one(pending, records, 2)
        self.assertEqual(records[0]["status"], "process_error")
        self.assertEqual(pending, [hung])
        started = time.perf_counter()
        while pending:
            runner._flush_one(pending, records, 0.3, wait=True)
        self.assertLess(time.perf_counter() - started, 5)
        self.assertEqual(records[1]["status"], "timeout")
        self.assertEqual(records[1]["returncode"], 124)
        self.assertIsNotNone(hung[0].poll())

    def test_missing_output_is_failure(self):
        child = self.launch("pass", "missing")
        child[0].wait(timeout=5)
        records = []
        runner._flush_one([child], records, 5)
        self.assertEqual(records[0]["status"], "stats_error")
        self.assertNotEqual(records[0]["returncode"], 0)

    def test_timeout_batch_preserves_reports_without_plotting(self):
        (self.root / "hung.json").write_text("{}")
        (self.root / "later.json").write_text("{}")
        with patch.object(runner, "find_executable", return_value=Path(sys.executable)), \
             patch.object(runner, "get_test_dir", return_value=self.root), \
             patch.object(runner, "build_command", return_value=[
                 sys.executable, "-c", "import time; time.sleep(60)"
             ]), \
             patch.object(runner, "plot_summary_dashboard", side_effect=ImportError("matplotlib")):
            started = time.perf_counter()
            self.assertEqual(runner.main([str(self.root), "-j", "1", "-t", "0.2"]), 1)
            self.assertLess(time.perf_counter() - started, 10)
        import json
        report = json.loads((self.root / "summary.json").read_text())
        self.assertEqual(report["failed_runs"], 2)
        self.assertEqual(report["graphs"], [])
        self.assertEqual(report["run_config"]["routing_metric"], "delay")
        self.assertEqual(report["run_config"]["seed"], 42)
        self.assertEqual(report["run_config"]["packet_size"], 1500)
        self.assertEqual(report["run_config"]["congestion_control"], "reno")
        self.assertEqual(report["run_config"]["link_events"], [])
        self.assertTrue(all(run["status"] == "timeout" for run in report["runs"]))
        self.assertTrue((self.root / "metrics.csv").exists())
        self.assertTrue((self.root / "run_config.json").exists())

    def test_mixed_batch_exit_and_reports(self):
        for name in ("ok", "bad"):
            (self.root / f"{name}.json").write_text("{}")
        def command(exe, topo, output, routing_metric="delay", seed=42, packet_size=1500, congestion_control="reno", link_events=None):
            if topo.stem == "bad":
                return [sys.executable, "-c", "raise SystemExit(7)"]
            content = ("packets_sent,packets_delivered,packets_lost,total_latency,avg_latency,"
                       "packets_in_transit,total_sessions,data_packets_delivered,schema_version,"
                       "simulation_duration_s,seed\n1,1,0,0,0,0,0,1,1,2,42\n")
            return [sys.executable, "-c",
                    f"from pathlib import Path; Path({str(output)!r}).write_text({content!r})"]
        with patch.object(runner, "find_executable", return_value=Path(sys.executable)), \
             patch.object(runner, "get_test_dir", return_value=self.root), \
             patch.object(runner, "build_command", side_effect=command), \
             patch.object(runner, "plot_summary_dashboard", return_value=self.root / "plot.png"):
            self.assertEqual(runner.main([str(self.root), "-j", "2", "-t", "5"]), 1)
        import json
        report = json.loads((self.root / "summary.json").read_text())
        self.assertEqual(report["successful_runs"], 1)
        self.assertEqual(report["failed_runs"], 1)
        self.assertTrue((self.root / "metrics.csv").exists())


if __name__ == "__main__":
    unittest.main()
