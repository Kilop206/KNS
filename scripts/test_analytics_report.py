import copy
import csv
from contextlib import redirect_stderr, redirect_stdout
from html.parser import HTMLParser
import io
import json
import os
from pathlib import Path
import tempfile
import unittest

import analytics_config
import analytics_report
import experiment_profile


def stats(value=None):
    n = int(value is not None)
    return {"count": n, "retained_samples": n, "discarded_samples": 0,
            "mean_s": value, "min_s": value, "max_s": value, "median_s": value, "p95_s": value, "p99_s": value,
            "sample_stddev_s": None, "percentile_scope": "all_observed_deliveries",
            "histogram": {"upper_edges_s": [0.001, 1], "counts": [0, n, 0], "scope": "all_observed_deliveries"}}


def fixture():
    samples = []
    for at in (0, 1):
        samples.append({"at_s": at, "from_s": 0, "duration_s": at, "packets_sent": at, "packets_delivered": at,
            "packets_lost": 0, "queue_overflow_drops": 0, "red_early_drops": 0,
            "throughput_pps": 1 if at else None, "first_hop_admission_pps": 1 if at else None,
            "drops_per_s": 0 if at else None, "data_latency": stats(0.01 if at else None),
            "links": [{"link_index": 0, "utilization_defined": bool(at), "utilization_complete": True,
                       "utilization": {"ab": 0.1 if at else None, "ba": 0 if at else None, "shared": None},
                       "max_queue_fraction": 0.1}]})
    group = {"packets_delivered": 1, "wire_bytes_delivered": 125, "data_latency": stats(0.01)}
    return {"schema_version": "1.0", "config": {"schema_version": "1.0", "until_s": 1, "interval_s": 1},
            "collection_until_s": 1, "simulation_end_s": 1, "pending_events": 0,
            "samples_seen": 2, "samples_discarded": 0, "samples": samples,
            "data_latency": stats(0.01), "packets_delivered_in_scope": 1, "wire_bytes_delivered_in_scope": 125,
            "flow_groups": [{"key": "1:0:1:0:0", **group}], "destination_node_groups": [{"key": "1", **copy.deepcopy(group)}],
            "ungrouped_flow_deliveries": 0, "ungrouped_node_deliveries": 0,
            "reservation_records_discarded": 0, "untracked_link_transmissions": 0, "recommendations": []}


class AnalyticsReportTests(unittest.TestCase):
    def test_link_mean_utilization_requires_a_valid_scope_and_fraction(self):
        report = fixture()
        report["link_summaries"] = [{"link_index": 0, "valid_utilization_duration_s": 1,
            "time_weighted_busiest_slot_utilization": 0.1}]
        analytics_config.validate_report(report, report["config"])
        for duration, mean in ((0, 0), (1, 1.1), (2, 0.1), (1, None)):
            invalid = copy.deepcopy(report)
            invalid["link_summaries"][0].update(valid_utilization_duration_s=duration, time_weighted_busiest_slot_utilization=mean)
            with self.assertRaises(ValueError): analytics_config.validate_report(invalid, invalid["config"])
        report["link_summaries"][0].update(valid_utilization_duration_s=0, time_weighted_busiest_slot_utilization=None)
        analytics_config.validate_report(report, report["config"])
    def test_tcp_goodput_conserves_payload_and_uses_virtual_duration(self):
        report = fixture()
        report.update(tcp_contiguous_payload_bytes=25, tcp_payload_goodput_bps=200,
                      ungrouped_flow_tcp_payload_bytes=0, ungrouped_node_tcp_payload_bytes=0)
        for key in ("flow_groups", "destination_node_groups"):
            report[key][0]["tcp_contiguous_payload_bytes"] = 25
        for sample in report["samples"]:
            sample["tcp_contiguous_payload_bytes"] = 25 if sample["at_s"] else 0
            sample["tcp_payload_goodput_bps"] = 200 if sample["at_s"] else None
        analytics_config.validate_report(report, report["config"])
        for mutate in (
            lambda r: r.update(tcp_payload_goodput_bps=201),
            lambda r: r.update(tcp_contiguous_payload_bytes=126),
            lambda r: r.update(ungrouped_flow_tcp_payload_bytes=1),
            lambda r: r["samples"][0].update(tcp_payload_goodput_bps=0),
            lambda r: r["samples"][1].update(tcp_payload_goodput_bps=199),
            lambda r: r["samples"][1].update(tcp_contiguous_payload_bytes=26),
            lambda r: r["flow_groups"][0].update(tcp_contiguous_payload_bytes=True),
        ):
            invalid = copy.deepcopy(report)
            mutate(invalid)
            with self.assertRaises(ValueError):
                analytics_config.validate_report(invalid, invalid["config"])
    def test_exports_legacy_report_without_inventing_variation(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "source.json"
            source.write_text(json.dumps(fixture()))
            original = source.read_bytes()
            output = root / "derived reports"
            with redirect_stdout(io.StringIO()):
                self.assertEqual(analytics_report.main([str(source), "--output", str(output)]), 0)
            self.assertEqual(source.read_bytes(), original)
            with (output / "series.csv").open(newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 2)
            self.assertEqual(rows[0]["throughput_pps"], "")
            self.assertEqual(rows[1]["variation_pairs"], "")
            with (output / "histograms.csv").open(newline="") as stream:
                self.assertEqual(sum(int(row["count"]) for row in csv.DictReader(stream)), 1)
            self.assertEqual(len(list(output.iterdir())), 6)

    def test_rejects_corrupt_metrics_before_creating_output(self):
        changes = [
            lambda r: r["data_latency"].update(mean_s=float("nan")),
            lambda r: r["data_latency"]["histogram"]["counts"].__setitem__(0, 1),
            lambda r: r["data_latency"].update(mean_s=2),
            lambda r: r["data_latency"].update(percentile_scope="most_recent_retained_deliveries"),
            lambda r: r["flow_groups"][0].update(packets_delivered=2),
            lambda r: r.update(data_latency=stats()),
            lambda r: r.update(wire_bytes_delivered_in_scope=126),
            lambda r: r["samples"][1]["links"][0]["utilization"].update(ab=1.1),
            lambda r: r["samples"][1].update(throughput_pps=9),
            lambda r: r["samples"][1].update(packets_sent=True),
            lambda r: r["samples"][1].update(from_s=0.5, duration_s=0.5),
            lambda r: r["samples"][0].update(drops_per_s=0),
            lambda r: r.update(collection_until_s=99),
        ]
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for mutation in changes:
                report = copy.deepcopy(fixture())
                mutation(report)
                source = root / "corrupt.json"
                source.write_text(json.dumps(report))
                output = root / "must not exist"
                with self.subTest(mutation=mutation), redirect_stderr(io.StringIO()):
                    self.assertEqual(analytics_report.main([str(source), "--output", str(output)]), 1)
                    self.assertFalse(output.exists())

    def test_incomplete_unknown_and_existing_destinations_are_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "source.json"
            output = root / "reports"
            output.mkdir()
            sentinel = output / "keep.txt"
            sentinel.write_text("previous evidence")
            for change in ({"pending_events": 1}, {"schema_version": "2.0"}, {}):
                report = {**fixture(), **change}
                source.write_text(json.dumps(report))
                with redirect_stderr(io.StringIO()):
                    self.assertEqual(analytics_report.main([str(source), "--output", str(output)]), 1)
                self.assertEqual(sentinel.read_text(), "previous evidence")
                self.assertEqual(list(output.iterdir()), [sentinel])

    def test_html_escapes_labels_and_does_not_connect_missing_samples(self):
        report = fixture()
        analytics_config.validate_report(report, report["config"])
        with tempfile.TemporaryDirectory() as temp:
            analytics_report.write(report, Path(temp), '<script>alert("x")</script>')
            html = (Path(temp) / "analytics.html").read_text(encoding="utf-8")
            HTMLParser().feed(html)
            self.assertNotIn("<script>", html)
            self.assertIn("&lt;script&gt;", html)
            self.assertIn("Content-Security-Policy", html)
            self.assertNotIn('src="http', html)
        gap = analytics_report.plot([(0, 0.1), (0.5, None), (1, 0.2)], "gaps")
        self.assertNotIn("polyline", gap)
        self.assertEqual(gap.count("<circle"), 2)

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for engine integration")
    def test_actual_engine_report_exports_and_replays_identically(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            bundle = root / "bundle"
            with redirect_stdout(io.StringIO()):
                self.assertEqual(experiment_profile.main(["--profile", str(experiment_profile.ROOT / "KNS Experiments/analytics-lab.json"),
                    "--exe", os.environ["KNS_TEST_EXE"], "--output", str(bundle)]), 0)
            benchmark = json.loads((bundle / "runs/benchmark.json").read_text())
            source = Path(benchmark["cases"][-1]["analytics_report"])
            report = json.loads(source.read_text())
            self.assertIn("data_latency_variation", report)
            self.assertGreater(report["tcp_contiguous_payload_bytes"], 0)
            self.assertTrue(any(rule["rule"] == "consecutive_serialization_queue_pressure" for rule in report["recommendations"]))
            original = source.read_bytes()
            for folder in ("report one", "report two"):
                with redirect_stdout(io.StringIO()):
                    self.assertEqual(analytics_report.main([str(source), "--output", str(root / folder)]), 0)
            self.assertEqual(source.read_bytes(), original)
            for first in (root / "report one").iterdir():
                self.assertEqual(first.read_bytes(), (root / "report two" / first.name).read_bytes())
            with (root / "report one/series.csv").open(newline="") as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 201)


if __name__ == "__main__":
    unittest.main()
