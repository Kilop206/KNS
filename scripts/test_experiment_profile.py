import copy
import csv
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import patch

import benchmark_suite
import experiment_profile as experiments


class ExperimentProfileTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="KNS experiments ")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.topology_path = self.root / "original.json"
        self.profile_path = self.root / "profile.json"
        self.output = self.root / "artifacts"
        self.topology = {
            "nodes": [{"id": 0}, {"id": 1}, {"id": 2}],
            "links": [
                {"from": 0, "to": 1, "bandwidth": 100, "delay": 5, "loss": 0},
                {"from": 1, "to": 2, "bandwidth": 100, "delay": 5, "loss": 0},
            ],
            "metadata": {"preserve": True},
        }
        self.profile = {
            "name": "comparison", "topology": str(self.topology_path), "index": 0,
            "experiments": [{"name": "baseline", "bandwidth": 100},
                            {"name": "changed", "bandwidth": 25, "delay": 10, "loss": 0.1}],
        }
        self.topology_path.write_text(json.dumps(self.topology), encoding="utf-8")
        self.save_profile()

    def save_profile(self):
        self.profile_path.write_text(json.dumps(self.profile), encoding="utf-8")

    def invoke(self, *arguments):
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            return experiments.main(["--profile", str(self.profile_path),
                                     "--output", str(self.output), *arguments])

    def test_generate_changes_only_selected_fields_and_keeps_source(self):
        original = self.topology_path.read_bytes()
        self.assertEqual(self.invoke("--generate-only"), 0)
        suite = benchmark_suite.load_suite(self.output / "suite.json")
        self.assertEqual(len(benchmark_suite.expand_cases(suite)), 2)
        for change, filename in zip(self.profile["experiments"], suite["topologies"]):
            expected = copy.deepcopy(self.topology)
            expected["links"][0].update({k: v for k, v in change.items() if k != "name"})
            self.assertEqual(json.loads(Path(filename).read_text(encoding="utf-8")), expected)
        self.assertEqual(self.topology_path.read_bytes(), original)
        self.assertEqual(json.loads((self.output / "source-topology.json").read_text()), self.topology)
        normalized = json.loads((self.output / "profile.json").read_text())
        self.assertEqual(normalized["seeds"], [42])
        self.assertEqual(normalized["baseline"], "baseline")

    def test_dry_run_expands_repetitions_without_files_or_executable(self):
        self.profile.update(seeds=[42, 43], packet_sizes=[1500, 3000],
                            routing_metrics=["delay", "bandwidth"])
        self.save_profile()
        with patch.object(experiments.runner, "find_executable", side_effect=AssertionError), \
                redirect_stdout(io.StringIO()) as stdout:
            result = experiments.main(["--profile", str(self.profile_path),
                                       "--output", str(self.output), "--dry-run"])
        self.assertEqual(result, 0)
        self.assertEqual(json.loads(stdout.getvalue())["case_count"], 16)
        self.assertFalse(self.output.exists())

    def test_invalid_profiles_do_not_create_artifacts(self):
        invalid = [
            {"name": "../escape"}, {"index": -1}, {"index": True}, {"index": 2},
            {"experiments": []}, {"experiments": [{"name": "empty"}]},
            {"experiments": [{"name": "bad", "bandwidth": 0}]},
            {"experiments": [{"name": "bad", "bandwidth": True}]},
            {"experiments": [{"name": "bad", "delay": -1}]},
            {"experiments": [{"name": "bad", "loss": 1.1}]},
            {"experiments": [{"name": "bad", "loss": float("nan")}]},
            {"experiments": [{"name": "bad", "bandwidth": float("inf")}]},
            {"experiments": [{"name": "bad", "bandwith": 10}]},
            {"experiments": [{"name": "same", "delay": 1}, {"name": "Same", "delay": 2}]},
            {"experiments": [{"name": "../../escape", "delay": 1}]},
            {"seeds": [True]}, {"seeds": [2**64]}, {"seeds": [42, 42]},
            {"seeds": "42"}, {"packet_sizes": [0]}, {"routing_metrics": [[]]},
            {"congestion_controls": ["unknown"]}, {"allow_transport_failures": 1},
            {"baseline": "missing"}, {"baseline": []},
        ]
        original = copy.deepcopy(self.profile)
        for changes in invalid:
            with self.subTest(changes=changes):
                self.profile = {**original, **changes}
                self.save_profile()
                self.assertEqual(self.invoke("--generate-only"), 1)
                self.assertFalse(self.output.exists())

    def test_missing_link_field_is_rejected(self):
        del self.topology["links"][0]["bandwidth"]
        self.topology_path.write_text(json.dumps(self.topology), encoding="utf-8")
        self.assertEqual(self.invoke("--generate-only"), 1)
        self.assertFalse(self.output.exists())

    def test_existing_output_and_source_directory_are_preserved(self):
        self.output.mkdir()
        sentinel = self.output / "keep.txt"
        sentinel.write_text("existing results")
        self.assertEqual(self.invoke("--generate-only"), 1)
        self.assertEqual(sentinel.read_text(), "existing results")
        self.assertEqual(list(self.output.iterdir()), [sentinel])

    def test_invalid_timeout_and_missing_executable_do_not_generate(self):
        for value in ("nan", "inf", "0", "-1"):
            with self.subTest(timeout=value):
                self.assertEqual(self.invoke("--generate-only", "--timeout", value), 1)
                self.assertFalse(self.output.exists())
        self.assertEqual(self.invoke("--exe", str(self.root / "missing.exe")), 1)
        self.assertFalse(self.output.exists())

    def test_startup_errors_keep_diagnostics_and_continue_all_cases(self):
        executable = self.root / "invalid executable"
        executable.write_text("not an executable")
        with patch.object(benchmark_suite.subprocess, "run", side_effect=OSError("cannot launch")):
            self.assertEqual(self.invoke("--exe", str(executable)), 1)
        report = json.loads((self.output / "runs" / "benchmark.json").read_text())
        self.assertEqual(report["failed_cases"], 2)
        for case in report["cases"]:
            self.assertEqual(case["status"], "startup_error")
            self.assertEqual(case["error"], "cannot launch")
        self.assertTrue((self.output / "runs" / "report.md").is_file())
        comparison = json.loads((self.output / "comparison.json").read_text())
        self.assertEqual(comparison["matched_pairs"], 0)
        self.assertEqual(comparison["unavailable_pairs"], 1)

    def test_timeout_and_malformed_stats_keep_reports_and_fail_batch(self):
        executable = self.root / "fake executable"
        executable.touch()
        outcomes = [subprocess.TimeoutExpired("KNS", 1), subprocess.CompletedProcess([], 0)]
        with patch.object(benchmark_suite.subprocess, "run", side_effect=outcomes):
            self.assertEqual(self.invoke("--exe", str(executable)), 1)
        report = json.loads((self.output / "runs" / "benchmark.json").read_text())
        self.assertEqual([c["status"] for c in report["cases"]], ["timeout", "stats_error"])
        with (self.output / "runs" / "metrics.csv").open(newline="") as stream:
            self.assertEqual(len(list(csv.DictReader(stream))), 2)

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for engine integration")
    def test_real_engine_runs_bundled_profile_from_another_directory(self):
        previous = Path.cwd()
        self.addCleanup(os.chdir, previous)
        os.chdir(self.root)
        with redirect_stdout(io.StringIO()):
            result = experiments.main(["--profile", str(experiments.DEFAULT_PROFILE),
                                       "--output", str(self.output),
                                       "--exe", os.environ["KNS_TEST_EXE"]])
        self.assertEqual(result, 0)
        report = json.loads((self.output / "runs" / "benchmark.json").read_text())
        self.assertEqual(report["successful_cases"], 3)
        self.assertTrue(all(case["packets_delivered"] > 0 for case in report["cases"]))
        comparison = json.loads((self.output / "comparison.json").read_text())
        self.assertEqual(comparison["matched_pairs"], 2)
        self.assertEqual(comparison["unavailable_pairs"], 0)


if __name__ == "__main__":
    unittest.main()
