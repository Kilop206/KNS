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
import experiment_manifest as manifests


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

    def test_version_and_matrix_bounds_are_validated_before_expansion(self):
        for update in ({"schema_version": "2.0"}, {"schema_version": 1},
                       {"max_cases": True}, {"max_cases": 0}, {"max_cases": 10001},
                       {"max_cases": 1}, {"packet_sizes": [2**31]},
                       {"seeds": list(range(501))}):
            with self.subTest(update=update):
                self.profile.update(update)
                self.save_profile()
                self.assertEqual(self.invoke("--generate-only"), 1)
                self.assertFalse(self.output.exists())
                for key in update:
                    self.profile.pop(key, None)
        self.profile["max_cases"] = 2
        self.save_profile()
        self.assertEqual(self.invoke("--generate-only"), 0)
        manifest = manifests.load(self.output / "manifest.json")
        self.assertEqual(manifest["status"], "prepared")
        self.assertEqual(manifest["configuration"]["profile"]["schema_version"], "1.0")

    def test_replay_survives_deleted_sources_and_moved_artifacts(self):
        self.assertEqual(self.invoke("--generate-only", "--timeout", "12"), 0)
        original = manifests.load(self.output / "manifest.json")
        self.profile_path.unlink()
        self.topology_path.unlink()
        moved = self.root / "moved artifacts"
        self.output.rename(moved)
        with redirect_stdout(io.StringIO()):
            code = experiments.main(["--replay", str(moved / "manifest.json"),
                                     "--output", str(self.output), "--generate-only"])
        self.assertEqual(code, 0)
        replay = manifests.load(self.output / "manifest.json")
        self.assertEqual(replay["configuration_sha256"], original["configuration_sha256"])
        self.assertEqual(replay["replay_of"], original["run_id"])
        self.assertNotEqual(replay["run_id"], original["run_id"])
        self.assertEqual(replay["configuration"]["timeout_seconds"], 12)
        self.assertEqual(json.loads((self.output / "source-topology.json").read_text()), self.topology)
        first = json.loads((moved / "topologies/001-baseline.json").read_text())
        second = json.loads((self.output / "topologies/001-baseline.json").read_text())
        self.assertEqual(first, second)

    def test_tampered_manifest_and_unknown_versions_do_not_create_output(self):
        profile, topology = experiments.load_profile(self.profile_path)
        manifest = manifests.create(profile, topology, 60, None, experiments.ROOT)
        path = self.root / "manifest.json"
        for mutate in (
            lambda x: x.update(schema_version="2.0"),
            lambda x: x["configuration"]["topology"].update(nodes=99),
            lambda x: x.update(run_id="invalid"),
            lambda x: x.update(executable_sha256="invalid"),
        ):
            broken = copy.deepcopy(manifest)
            mutate(broken)
            experiments.write_json(path, broken)
            with redirect_stderr(io.StringIO()):
                code = experiments.main(["--replay", str(path), "--output", str(self.output),
                                         "--generate-only"])
            self.assertEqual(code, 1)
            self.assertFalse(self.output.exists())

    def test_replay_binary_changes_require_explicit_override(self):
        profile, topology = experiments.load_profile(self.profile_path)
        binary = self.root / "kns binary"
        binary.write_bytes(b"original")
        original = manifests.create(profile, topology, 60, binary, experiments.ROOT)
        binary.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "executable SHA-256 differs"):
            manifests.create(profile, topology, 60, binary, experiments.ROOT, original)
        changed = manifests.create(profile, topology, 60, binary, experiments.ROOT, original, True)
        self.assertTrue(changed["executable_changed"])
        self.assertNotEqual(changed["executable_sha256"], original["executable_sha256"])
        self.assertEqual(changed["configuration_sha256"], original["configuration_sha256"])

    def test_existing_manifest_is_never_overwritten_on_generation_failure(self):
        self.output.mkdir()
        sentinel = self.output / "manifest.json"
        sentinel.write_text("prior evidence")
        self.assertEqual(self.invoke("--generate-only"), 1)
        self.assertEqual(sentinel.read_text(), "prior evidence")

    def test_hash_ignores_key_order_but_tracks_model_and_seed_changes(self):
        profile, topology = experiments.load_profile(self.profile_path)
        first = manifests.create(profile, topology, 60, None, experiments.ROOT)
        reordered = manifests.create(dict(reversed(list(profile.items()))), topology, 60, None, experiments.ROOT)
        self.assertEqual(first["configuration_sha256"], reordered["configuration_sha256"])
        profile["seeds"] = [99]
        different = manifests.create(profile, topology, 60, None, experiments.ROOT)
        self.assertNotEqual(first["configuration_sha256"], different["configuration_sha256"])

    def test_sweep_ranges_and_lists_expand_stably_and_preserve_source(self):
        del self.profile["experiments"]
        self.profile["sweep"] = {"bandwidth": [10, 100], "delay": {"start": 0.1, "stop": 0.3, "step": 0.1},
                                 "queue_capacity": [2, 4]}
        self.profile["seeds"] = [1, 2]
        self.save_profile()
        self.assertEqual(self.invoke("--generate-only"), 0)
        saved = json.loads((self.output / "profile.json").read_text())
        self.assertNotIn("sweep", saved)
        self.assertEqual(len(saved["experiments"]), 12)
        self.assertEqual(saved["experiments"][0], {"name": "sweep-0001", "bandwidth": 10,
                                                   "delay": 0.1, "queue_capacity": 2})
        self.assertEqual(saved["experiments"][-1]["delay"], 0.3)
        suite = benchmark_suite.load_suite(self.output / "suite.json")
        self.assertEqual(len(benchmark_suite.expand_cases(suite)), 24)
        self.assertNotIn("queue_capacity", self.topology["links"][0])
        generated = json.loads(Path(suite["topologies"][0]).read_text())
        self.assertEqual(generated["links"][0]["queue_capacity"], 2)

    def test_invalid_and_explosive_sweeps_are_rejected_before_writing(self):
        original = copy.deepcopy(self.profile)
        for sweep in ({}, {"unknown": [1]}, {"delay": []}, {"delay": [True]},
                      {"delay": [1, 1]}, {"delay": {"start": 0, "stop": 1, "step": 0}},
                      {"delay": {"start": 2, "stop": 1, "step": 1}},
                      {"delay": {"start": 0, "stop": 1e20, "step": 1e-10}},
                      {"delay": list(range(40)), "bandwidth": list(range(1, 41))},
                      {"queue_capacity": [1.5]}, {"loss": [1.1]}):
            with self.subTest(sweep=sweep):
                self.profile = {key: value for key, value in original.items() if key != "experiments"}
                self.profile["sweep"] = sweep
                self.save_profile()
                self.assertEqual(self.invoke("--generate-only"), 1)
                self.assertFalse(self.output.exists())
        self.profile = original
        self.profile["sweep"] = {"delay": [1]}
        self.save_profile()
        self.assertEqual(self.invoke("--generate-only"), 1)

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
        manifest = manifests.load(self.output / "manifest.json")
        self.assertEqual(manifest["status"], "failed")
        self.assertEqual(manifest["failed_cases"], 2)

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
        manifest = manifests.load(self.output / "manifest.json")
        self.assertEqual(manifest["status"], "completed")
        replay_output = self.root / "replayed"
        with redirect_stdout(io.StringIO()):
            self.assertEqual(experiments.main(["--replay", str(self.output / "manifest.json"),
                                               "--output", str(replay_output),
                                               "--exe", os.environ["KNS_TEST_EXE"]]), 0)
        replay = manifests.load(replay_output / "manifest.json")
        self.assertEqual(manifest["configuration_sha256"], replay["configuration_sha256"])
        for source_csv in (self.output / "runs").glob("*.csv"):
            if source_csv.name != "metrics.csv":
                self.assertEqual(source_csv.read_bytes(), (replay_output / "runs" / source_csv.name).read_bytes())


if __name__ == "__main__":
    unittest.main()
