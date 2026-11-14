from contextlib import redirect_stderr, redirect_stdout
import copy
import io
import json
import os
from pathlib import Path
import tempfile
import unittest

import experiment_profile
import trace_import
import traffic_config

ROOT = Path(__file__).resolve().parent.parent
LAB = ROOT / "KNS Experiments/trace-lab"


class TraceImportTests(unittest.TestCase):
    def setUp(self):
        self.topology = json.loads((LAB / "topology.json").read_text())

    def test_csv_json_preserve_ties_sizes_and_suppress_original_labels(self):
        plans = []
        for format_name in ("csv", "json"):
            plan, metadata = trace_import.convert((LAB / f"packets.{format_name}").read_bytes(), format_name, self.topology)
            traffic_config.validate(plan, self.topology)
            self.assertEqual([item["at_s"] for item in plan["replay_events"]], [0, 0, 0, 0.01, 0.02])
            self.assertEqual([item["flow_id"] for item in plan["replay_events"]], [1, 1, 2, 3, 4])
            self.assertEqual(metadata["source_flow_count"], 2)
            self.assertEqual([group["source_flow_id"] for group in metadata["wire_groups"]], [1, 2, 1, 2])
            self.assertEqual(metadata["wire_bytes_total"], 875)
            self.assertEqual(sum(metadata["interarrival_s"]["histogram"]["counts"]), 4)
            self.assertEqual(metadata["interarrival_s"]["mean"], 0.005)
            self.assertNotIn("private-sensor", json.dumps((plan, metadata)))
            self.assertNotIn("private-api", json.dumps((plan, metadata)))
            plans.append(plan)
        self.assertEqual(plans[0], plans[1])
        _, metadata = trace_import.convert((LAB / "packets.csv").read_bytes(), "csv", self.topology, keep_flow_labels=True)
        self.assertIn("private-sensor", json.dumps(metadata))

    def test_nan_duplicates_unsorted_unknown_nodes_limits_and_existing_output_fail_safely(self):
        valid = json.loads((LAB / "packets.json").read_text())
        mutations = [lambda d: d.update(packets=[]), lambda d: d["packets"][1].update(time_s=-1),
                     lambda d: d["packets"][0].update(source=99), lambda d: d["packets"][0].update(wire_bytes=0),
                     lambda d: d["packets"][0].update(source=True), lambda d: d["packets"][0].update(flow_id=""),
                     lambda d: d["packets"][0].update(time_s=float("nan")),
                     lambda d: d["packets"][0].update(payload="forbidden"),
                     lambda d: d.update(packets=d["packets"] * 2001)]
        for mutate in mutations:
            bad = copy.deepcopy(valid); mutate(bad)
            with self.assertRaises(ValueError): trace_import.convert(json.dumps(bad).encode(), "json", self.topology)
        with self.assertRaises(ValueError): trace_import.convert(b'{"schema_version":"1.0","schema_version":"2.0","packets":[]}', "json", self.topology)
        with self.assertRaises(ValueError): trace_import.convert(b"x" * (trace_import.LIMIT + 1), "csv", self.topology)
        with self.assertRaises(ValueError): trace_import.convert(b"time_s,flow_id,source,destination,wire_bytes,wire_bytes\n", "csv", self.topology)
        with self.assertRaises(ValueError): trace_import.convert(b"pcap", "pcap", self.topology)
        too_many_groups = {"schema_version": "1.0", "packets": [
            {"time_s": 0, "flow_id": "one-flow", "source": 0, "destination": 1, "wire_bytes": size} for size in range(1, 258)]}
        with self.assertRaisesRegex(ValueError, "256"):
            trace_import.convert(json.dumps(too_many_groups).encode(), "json", self.topology)
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "existing"; output.mkdir()
            sentinel = output / "keep.txt"; sentinel.write_text("evidence")
            with redirect_stderr(io.StringIO()):
                self.assertEqual(trace_import.main([str(LAB / "packets.csv"), "--topology", str(LAB / "topology.json"), "--output", str(output)]), 1)
            self.assertEqual(sentinel.read_text(), "evidence")
            self.assertEqual(list(output.iterdir()), [sentinel])

    def test_decimal_normalization_retains_nanoseconds_and_reports_clock_collisions(self):
        def rows(times):
            return ("time_s,flow_id,source,destination,wire_bytes\n" + "".join(f"{at},flow,0,1,125\n" for at in times)).encode()
        plan, meta = trace_import.convert(rows(["1700000000.000000001", "1700000000.000000002"]), "csv", self.topology)
        self.assertEqual(plan["replay_events"][1]["at_s"], 1e-9)
        self.assertEqual(meta["floating_time_collisions"], 0)
        _, meta = trace_import.convert(rows(["0", "999999", "999999.000000000001"]), "csv", self.topology)
        self.assertEqual(meta["floating_time_collisions"], 1)

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for actual trace replay")
    def test_actual_import_uses_network_queues_and_manifest_replay_survives_deleted_trace(self):
        with tempfile.TemporaryDirectory(prefix="KNS trace ") as folder:
            root = Path(folder)
            source = root / "private capture.csv"; original = (LAB / "packets.csv").read_bytes(); source.write_bytes(original)
            output = root / "import bundle"
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(trace_import.main([str(source), "--topology", str(LAB / "topology.json"), "--output", str(output), "--exe", os.environ["KNS_TEST_EXE"]]), 0)
            self.assertEqual(source.read_bytes(), original)
            self.assertFalse((output / source.name).exists())
            for path in output.glob("*.json"):
                self.assertNotIn("private-sensor", path.read_text())
            benchmark = json.loads((output / "experiment/runs/benchmark.json").read_text())
            report_path = Path(benchmark["cases"][0]["traffic_report"])
            report = json.loads(report_path.read_text())
            traffic_config.validate_report(report, report["config"])
            self.assertEqual((report["generated"], report["delivered"], report["dropped"]), (5, 4, 1))
            self.assertEqual(report["flows"][1]["dropped"], 1)
            self.assertEqual(sum(f["delivered_wire_bytes"] for f in report["flows"]), 625)
            source.unlink()
            replay = root / "portable replay"
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(experiment_profile.main(["--replay", str(output / "experiment/manifest.json"), "--exe", os.environ["KNS_TEST_EXE"], "--output", str(replay)]), 0)
            for path in (output / "experiment/runs").glob("*.traffic.json"):
                self.assertEqual(path.read_bytes(), (replay / "runs" / path.name).read_bytes())


if __name__ == "__main__":
    unittest.main()
