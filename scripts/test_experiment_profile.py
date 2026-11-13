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

    def test_invalid_fault_plans_do_not_create_artifacts(self):
        plan = {"schema_version": "1.0", "in_flight_policy": "complete_accepted_hops",
                "events": [], "probes": [{"source": 0, "destination": 2}], "observe_at": [0, 1]}
        for update in ({"in_flight_policy": "discard_all"}, {"schema_version": "2.0"},
                       {"observe_at": [True]}, {"probes": [{"source": 0, "destination": 0}]},
                       {"events": [{"at": 0, "type": "node_down", "link_index": 0}]},
                       {"events": [{"at": 0, "type": "link_down", "link_index": True}]},
                       {"events": [{"at": 0, "type": "link_down", "link_index": 9}]},
                       {"events": [{"at": 0, "type": "loss_probability", "link_index": 0, "value": 2}]}):
            self.profile["experiments"][1]["fault_plan"] = {**plan, **update}
            self.save_profile()
            with self.subTest(update=update):
                self.assertEqual(self.invoke("--generate-only"), 1)
                self.assertFalse(self.output.exists())

    def test_invalid_analytics_configs_do_not_create_artifacts(self):
        valid = {"schema_version": "1.0", "until_s": 0.1, "interval_s": 0.01}
        for update in ({"schema_version": "2.0"}, {"interval_s": 0}, {"until_s": -1},
                       {"until_s": float("nan")}, {"interval_s": 1e-30},
                       {"max_samples": True}, {"latency_retention": 0}, {"max_groups": 1025}):
            self.profile["analytics"] = {**valid, **update}
            self.save_profile()
            with self.subTest(update=update):
                self.assertEqual(self.invoke("--generate-only"), 1)
                self.assertFalse(self.output.exists())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for engine integration")
    def test_real_analytics_retention_replay_and_unchanged_simulation_csv(self):
        config = {"schema_version": "1.0", "until_s": 0.1, "interval_s": 0.001,
                  "max_samples": 16, "latency_retention": 2, "max_groups": 1, "group_retention": 1}
        self.profile["analytics"] = config
        self.save_profile()
        self.assertEqual(self.invoke("--exe", os.environ["KNS_TEST_EXE"]), 0)
        benchmark = json.loads((self.output / "runs/benchmark.json").read_text())
        first = json.loads(Path(benchmark["cases"][0]["analytics_report"]).read_text())
        self.assertEqual(first["samples_seen"], 101)
        self.assertEqual(len(first["samples"]), 16)
        self.assertEqual(first["samples_discarded"], 85)
        self.assertGreater(first["data_latency"]["count"], 2)
        self.assertEqual(first["data_latency"]["retained_samples"], 2)
        self.assertEqual(first["data_latency"]["percentile_scope"], "most_recent_retained_deliveries")
        self.assertEqual(sum(first["data_latency"]["histogram"]["counts"]), first["data_latency"]["count"])
        broken = copy.deepcopy(first)
        broken["samples_seen"] += 1
        with self.assertRaises(ValueError):
            experiments.analytics_config.validate_report(broken, config)
        replay = self.root / "analytics replay"
        with redirect_stdout(io.StringIO()):
            self.assertEqual(experiments.main(["--replay", str(self.output / "manifest.json"),
                "--exe", os.environ["KNS_TEST_EXE"], "--output", str(replay)]), 0)
        for original in (self.output / "runs").glob("*.analytics.json"):
            self.assertEqual(original.read_bytes(), (replay / "runs" / original.name).read_bytes())
        del self.profile["analytics"]
        self.save_profile()
        plain = self.root / "plain"
        with redirect_stdout(io.StringIO()):
            self.assertEqual(experiments.main(["--profile", str(self.profile_path), "--exe", os.environ["KNS_TEST_EXE"],
                                              "--output", str(plain)]), 0)
        for original in (self.output / "runs").glob("*.csv"):
            if original.name != "metrics.csv":
                self.assertEqual(original.read_bytes(), (plain / "runs" / original.name).read_bytes())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for engine integration")
    def test_cli_rejects_invalid_analytics_before_export(self):
        path = self.root / "invalid analytics.json"
        path.write_text('{"schema_version":"1.0","until_s":1,"interval_s":0}')
        output = self.root / "invalid.csv"
        result = subprocess.run([os.environ["KNS_TEST_EXE"], "--headless", "--topology", str(self.topology_path),
            "--analytics", str(path), "--output", str(output)], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Invalid analytics config", result.stderr)
        self.assertFalse(output.exists())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for engine integration")
    def test_real_fault_plan_reports_outage_and_replays_identical_observations(self):
        plan = {"schema_version": "1.0", "in_flight_policy": "complete_accepted_hops",
                "events": [{"at": 0.02, "type": "link_down", "link_index": 0},
                           {"at": 0.06, "type": "link_up", "link_index": 0}],
                "probes": [{"source": 0, "destination": 2}], "observe_at": [0, 0.01, 0.02, 0.04, 0.06, 0.1]}
        self.profile["experiments"][0]["fault_plan"] = {**plan, "events": []}
        self.profile["experiments"][1] = {"name": "outage", "fault_plan": plan}
        self.save_profile()
        self.assertEqual(self.invoke("--exe", os.environ["KNS_TEST_EXE"]), 0)
        benchmark = json.loads((self.output / "runs/benchmark.json").read_text())
        reports = [json.loads(Path(case["fault_report"]).read_text()) for case in benchmark["cases"]]
        self.assertEqual(reports[0]["sampled_connectivity_outages"], [])
        self.assertEqual(len(reports[1]["changes"]), 2)
        self.assertAlmostEqual(reports[1]["sampled_connectivity_outages"][0]["duration_s"], 0.04)
        self.assertIsNone(reports[1]["service_recovery_s"])
        broken = copy.deepcopy(reports[1])
        broken["changes"] = []
        with self.assertRaisesRegex(ValueError, "missing applied events"):
            experiments.fault_plan.validate_report(broken, plan)
        replay = self.root / "fault replay"
        with redirect_stdout(io.StringIO()):
            self.assertEqual(experiments.main(["--replay", str(self.output / "manifest.json"),
                "--exe", os.environ["KNS_TEST_EXE"], "--output", str(replay)]), 0)
        for original in (self.output / "runs").glob("*.faults.json"):
            self.assertEqual(original.read_bytes(), (replay / "runs" / original.name).read_bytes())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for engine integration")
    def test_cli_rejects_invalid_fault_plan_without_export(self):
        path = self.root / "bad fault plan.json"
        path.write_text('{"schema_version":"1.0"}')
        csv_path = self.root / "must not exist.csv"
        result = subprocess.run([os.environ["KNS_TEST_EXE"], "--headless", "--topology", str(self.topology_path),
                                 "--fault-plan", str(path), "--output", str(csv_path)],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Invalid fault plan", result.stderr)
        self.assertFalse(csv_path.exists())

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

    def test_traffic_plan_is_embedded_and_inactive_tcp_sweeps_are_rejected(self):
        lab = experiments.ROOT / "KNS Experiments/traffic-lab"
        self.profile["traffic_plan"] = json.loads((lab / "traffic.json").read_text())
        self.save_profile()
        self.assertEqual(self.invoke("--generate-only"), 0)
        self.assertEqual(json.loads((self.output / "traffic.json").read_text()), self.profile["traffic_plan"])
        suite = benchmark_suite.load_suite(self.output / "suite.json")
        self.assertTrue(all(case["traffic_plan"] == str(self.output / "traffic.json") for case in benchmark_suite.expand_cases(suite)))
        self.assertEqual(manifests.load(self.output / "manifest.json")["configuration"]["profile"]["traffic_plan"], self.profile["traffic_plan"])
        for field, values in (("packet_sizes", [500, 1000]), ("congestion_controls", ["reno", "cubic"])):
            invalid = copy.deepcopy(self.profile); invalid[field] = values
            with self.assertRaisesRegex(ValueError, "sweeps are unsupported"):
                experiments.validate_profile(invalid, self.topology)

    def test_traffic_sweeps_expand_joint_axes_without_modifying_base_plan(self):
        lab = experiments.ROOT / "KNS Experiments/traffic-lab"
        self.profile["traffic_plan"] = json.loads((lab / "traffic.json").read_text())
        original = copy.deepcopy(self.profile["traffic_plan"])
        del self.profile["experiments"]
        self.profile["sweep"] = {"bandwidth": [1, 10], "traffic": [
            {"flow_id": 2, "parameter": "schedule.rate_bps", "values": [600000, 1200000]},
            {"flow_id": 2, "parameter": "wire_bytes", "values": {"start": 500, "stop": 1000, "step": 500}}]}
        self.profile["max_cases"] = 8
        self.save_profile()
        self.assertEqual(self.invoke("--generate-only"), 0)
        suite = benchmark_suite.load_suite(self.output / "suite.json")
        self.assertEqual(len(suite["traffic_plans"]), 8)
        self.assertEqual(self.profile["traffic_plan"], original)
        normalized = json.loads((self.output / "profile.json").read_text())
        for variant, topology in zip(normalized["experiments"], suite["topologies"]):
            plan = json.loads(Path(suite["traffic_plans"][topology]).read_text())
            changes = {item["parameter"]: item["value"] for item in variant["traffic_overrides"]}
            self.assertEqual(plan["flows"][1]["wire_bytes"], changes["wire_bytes"])
            self.assertEqual(plan["flows"][1]["schedule"]["rate_bps"], changes["schedule.rate_bps"])
        self.assertTrue(all("traffic_plan" in case for case in benchmark_suite.expand_cases(suite)))

    def test_invalid_traffic_axes_fail_before_writing_artifacts(self):
        lab = experiments.ROOT / "KNS Experiments/traffic-lab"
        original = copy.deepcopy(self.profile)
        original["traffic_plan"] = json.loads((lab / "traffic.json").read_text())
        del original["experiments"]
        axes = [
            [{"flow_id": 999, "parameter": "wire_bytes", "values": [100]}],
            [{"flow_id": 1, "parameter": "schedule.rate_bps", "values": [100]}],
            [{"flow_id": 1, "parameter": "wire_bytes", "values": [0]}],
            [{"flow_id": 1, "parameter": "wire_bytes", "values": [10**1000]}],
            [{"flow_id": 1, "parameter": "schedule.interval_s", "values": [0]}],
            [{"flow_id": 1, "parameter": "source", "values": [1]}],
            [{"flow_id": 1, "parameter": "wire_bytes", "values": [100]}] * 2,
            [{"flow_id": 2, "parameter": "wire_bytes", "values": list(range(1, 1001))},
             {"flow_id": 2, "parameter": "schedule.rate_bps", "values": list(range(1, 1001))}],
        ]
        for traffic in axes:
            self.profile = copy.deepcopy(original); self.profile["sweep"] = {"traffic": traffic}
            self.save_profile()
            self.assertEqual(self.invoke("--generate-only"), 1)
            self.assertFalse(self.output.exists())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for engine integration")
    def test_real_joint_traffic_sweep_replays_distinct_offered_loads(self):
        lab = experiments.ROOT / "KNS Experiments/traffic-lab"
        self.profile = {"schema_version": "1.0", "name": "joint-sweep", "topology": str(lab / "topology.json"),
            "index": 1, "max_cases": 4, "traffic_plan": json.loads((lab / "traffic.json").read_text()),
            "sweep": {"bandwidth": [1, 10], "traffic": [{"flow_id": 2, "parameter": "schedule.rate_bps", "values": [600000, 1200000]}]}}
        self.save_profile()
        self.assertEqual(self.invoke("--exe", os.environ["KNS_TEST_EXE"]), 0)
        suite = benchmark_suite.load_suite(self.output / "suite.json")
        self.assertEqual(len(suite["traffic_plans"]), 4)
        cases = json.loads((self.output / "runs/benchmark.json").read_text())["cases"]
        counts = {json.loads(Path(case["traffic_report"]).read_text())["generated"] for case in cases}
        self.assertEqual(len(counts), 2)
        self.assertTrue(all(case["status"] == "success" for case in cases))
        comparison = json.loads((self.output / "comparison.json").read_text())
        self.assertFalse(comparison["methodology"]["traffic_plan_inputs_match"])
        self.assertEqual(len(set(comparison["methodology"]["traffic_plan_input_hashes"].values())), 2)
        replay = self.root / "sweep replay"
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(experiments.main(["--replay", str(self.output / "manifest.json"), "--exe", os.environ["KNS_TEST_EXE"], "--output", str(replay)]), 0)
        for source in (self.output / "runs").glob("*.traffic.json"):
            self.assertEqual(source.read_bytes(), (replay / "runs" / source.name).read_bytes())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE to a built engine for integration")
    def test_real_traffic_manifest_replay_preserves_all_raw_outcomes(self):
        lab = experiments.ROOT / "KNS Experiments/traffic-lab"
        self.profile = {"schema_version": "1.0", "name": "traffic-replay", "topology": str(lab / "topology.json"),
            "index": 1, "seeds": [42, 43], "traffic_plan": json.loads((lab / "traffic.json").read_text()),
            "analytics": json.loads((lab / "analytics.json").read_text()),
            "experiments": [{"name": "control", "bandwidth": 1}, {"name": "upgrade", "bandwidth": 10}]}
        self.save_profile()
        self.assertEqual(self.invoke("--exe", os.environ["KNS_TEST_EXE"]), 0)
        benchmark = json.loads((self.output / "runs/benchmark.json").read_text())
        self.assertEqual(benchmark["successful_cases"], 4)
        self.assertTrue(all(case["traffic_report"] and case["workload_model"] == "abstract_wire" for case in benchmark["cases"]))
        comparison = json.loads((self.output / "comparison.json").read_text())
        self.assertEqual(comparison["matched_pairs"], 2)
        self.assertEqual(comparison["methodology"]["workload_model"], "abstract_wire")
        replay = self.root / "traffic replay"
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(experiments.main(["--replay", str(self.output / "manifest.json"), "--exe", os.environ["KNS_TEST_EXE"], "--output", str(replay)]), 0)
        for pattern in ("*.csv", "*.traffic.json", "*.analytics.json"):
            for original in (self.output / "runs").glob(pattern):
                if original.name != "metrics.csv":
                    self.assertEqual(original.read_bytes(), (replay / "runs" / original.name).read_bytes())

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
