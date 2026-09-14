import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

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
    def test_engine_csv(self):
        root = Path(__file__).resolve().parent.parent
        subprocess.run(runner.build_command(Path(os.environ["KNS_TEST_EXE"]),
                       root / "app/topologies/mesh4.json", self.path),
                       env={**os.environ, "KNS_AUTO_START": "1"}, check=True, timeout=30)
        stats = runner.parse_stats(self.path)
        self.assertGreater(stats["packets_delivered"], 0)
        self.assertGreater(stats["simulation_duration_s"], 0)


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

    def test_mixed_batch_exit_and_reports(self):
        for name in ("ok", "bad"):
            (self.root / f"{name}.json").write_text("{}")
        def command(exe, topo, output):
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
