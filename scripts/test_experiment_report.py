import json
import tempfile
import unittest
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit

import experiment_report


class ReportParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []
        self.tags = []
        self.content = []

    def handle_starttag(self, tag, attrs):
        self.tags.append(tag)
        if tag == "a":
            self.links.append(dict(attrs)["href"])

    def handle_data(self, data):
        self.content.append(data)


class ExperimentReportTests(unittest.TestCase):
    def test_moved_bundle_has_local_links_and_escapes_recorded_content(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "original"
            (output / "runs").mkdir(parents=True)
            filename = "case #1 & metrics.csv"
            (output / "runs" / filename).write_text("raw evidence")
            hostile = '<img src=x onerror="alert(1)">'
            benchmark = {
                "case_count": 1,
                "cases": [{
                    "case_id": hostile,
                    "status": "startup_error",
                    "error": hostile,
                    "csv": str(output / "runs" / filename),
                    "log": None,
                }],
            }
            (output / "runs/benchmark.json").write_text(json.dumps(benchmark))
            manifest = {
                "configuration": {"profile": {"name": hostile, "baseline": "control"}},
                "status": "failed",
                "returncode": 1,
                "source_revision": hostile,
            }
            # Move before rendering while the recorded CSV path remains absolute.
            moved = root / "moved bundle"
            output.rename(moved)
            experiment_report.write(
                moved, manifest, {"matched_pairs": 0, "expected_pairs": 1}, None
            )
            parser = ReportParser()
            parser.feed((moved / "report.html").read_text(encoding="utf-8"))
            self.assertNotIn("img", parser.tags)
            self.assertNotIn("script", parser.tags)
            self.assertIn(hostile, "".join(parser.content))
            self.assertIn("No validation rules configured.", "".join(parser.content))
            self.assertIn("runs/case%20%231%20%26%20metrics.csv", parser.links)
            for href in parser.links:
                self.assertEqual(urlsplit(href).scheme, "")
                self.assertTrue((moved / unquote(href)).is_file())

    def test_verdict_path_cannot_link_outside_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "bundle"
            (output / "runs").mkdir(parents=True)
            (root / "outside.html").write_text("external")
            (output / "runs/benchmark.json").write_text(json.dumps({
                "case_count": 1,
                "cases": [{"case_id": "one", "status": "success"}],
            }))
            manifest = {
                "configuration": {"profile": {"name": "sample", "baseline": "one"}},
                "status": "failed", "returncode": 2,
            }
            validation = {
                "passed_cases": 0, "failed_cases": 1, "error_cases": 0,
                "cases": [{"case_id": "one", "status": "failed",
                           "failed_assertions": 1, "report": "../outside.json"}],
            }
            experiment_report.write(
                output, manifest, {"matched_pairs": 0, "expected_pairs": 0}, validation
            )
            parser = ReportParser()
            parser.feed((output / "report.html").read_text(encoding="utf-8"))
            self.assertEqual(parser.links, ["runs/benchmark.json"])
            self.assertIn("1 failed assertions", "".join(parser.content))


if __name__ == "__main__":
    unittest.main()
