import io
import json
import os
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch
from urllib.parse import unquote

import analyze_experiment
import benchmark_suite
import experiment_manifest as manifests
import experiment_profile
from test_experiment_report import ReportParser


class SavedExperimentAnalysisTests(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for engine integration")
    def test_moved_bundle_is_revalidated_without_simulation_or_source_mutation(self):
        with tempfile.TemporaryDirectory(prefix="KNS saved analysis ") as directory:
            root = Path(directory)
            source = root / "original"
            output = root / "analysis"
            lab = experiment_profile.ROOT / "KNS Experiments/traffic-lab"
            profile = json.loads((lab / "profile.json").read_text())
            profile["baseline"] = "upgrade"
            profile["validation"] = {
                "schema_version": "1.2",
                "assertions": [
                    {"metric": "delivery_ratio", "operator": ">=", "baseline_percent": -5}
                ],
            }
            profile_path = root / "profile.json"
            experiment_profile.write_json(profile_path, profile)
            with redirect_stdout(io.StringIO()):
                self.assertEqual(experiment_profile.main([
                    "--profile", str(profile_path), "--output", str(source),
                    "--exe", os.environ["KNS_TEST_EXE"],
                ]), 2)
            profile_path.unlink()
            moved = root / "moved experiment"
            source.rename(moved)
            before = {
                path.relative_to(moved): manifests.file_hash(path)
                for path in moved.rglob("*") if path.is_file()
            }
            with (
                redirect_stdout(io.StringIO()),
                patch.object(benchmark_suite.subprocess, "run", side_effect=AssertionError("Simulator must not run")),
            ):
                self.assertEqual(analyze_experiment.main([
                    str(moved), "--output", str(output)
                ]), 2)
            after = {
                path.relative_to(moved): manifests.file_hash(path)
                for path in moved.rglob("*") if path.is_file()
            }
            self.assertEqual(before, after)
            analysis = json.loads((output / "analysis.json").read_text())
            self.assertEqual(analysis["source_manifest_sha256"], before[Path("manifest.json")])
            self.assertEqual(analysis["source_benchmark_sha256"], before[Path("runs/benchmark.json")])
            summary = json.loads((output / "validation/summary.json").read_text())
            self.assertEqual((summary["passed_cases"], summary["failed_cases"], summary["error_cases"]), (2, 2, 0))
            parser = ReportParser()
            parser.feed((output / "report.html").read_text(encoding="utf-8"))
            self.assertIn("Saved-result analysis", "".join(parser.content))
            self.assertGreater(len(parser.links), 25)
            for href in parser.links:
                self.assertTrue((output / unquote(href)).is_file(), href)

            # Existing destinations and incomplete case matrices cannot be replaced.
            with redirect_stderr(io.StringIO()):
                self.assertEqual(analyze_experiment.main([
                    str(moved), "--output", str(output)
                ]), 1)
            self.assertEqual(
                analysis, json.loads((output / "analysis.json").read_text())
            )
            benchmark = json.loads((moved / "runs/benchmark.json").read_text())
            report_path = manifests.case_artifact(
                moved, benchmark["cases"][0]["traffic_report"]
            )
            traffic = json.loads(report_path.read_text())
            traffic["config"]["seed"] = 999
            experiment_profile.write_json(report_path, traffic)
            tampered_output = root / "changed workload"
            with redirect_stdout(io.StringIO()):
                self.assertEqual(analyze_experiment.main([
                    str(moved), "--output", str(tampered_output)
                ]), 1)
            verdicts = json.loads((tampered_output / "validation/summary.json").read_text())
            self.assertEqual(verdicts["error_cases"], 1)
            self.assertIn("differs from the saved profile", verdicts["cases"][0]["error"])
            benchmark_path = moved / "runs/benchmark.json"
            benchmark = json.loads(benchmark_path.read_text())
            original_id = benchmark["cases"][0]["case_id"]
            for invalid_id in ("../../escape", benchmark["cases"][1]["case_id"].upper()):
                benchmark["cases"][0]["case_id"] = invalid_id
                experiment_profile.write_json(benchmark_path, benchmark)
                rejected = root / "rejected"
                with redirect_stderr(io.StringIO()) as errors:
                    self.assertEqual(analyze_experiment.main([
                        str(moved), "--output", str(rejected)
                    ]), 1)
                self.assertIn("Case IDs", errors.getvalue())
                self.assertFalse(rejected.exists())
                self.assertFalse((root / "escape").exists())
            benchmark["cases"][0]["case_id"] = original_id
            benchmark["cases"].pop()
            benchmark["case_count"] -= 1
            experiment_profile.write_json(benchmark_path, benchmark)
            incomplete_output = root / "incomplete"
            with redirect_stderr(io.StringIO()):
                self.assertEqual(analyze_experiment.main([
                    str(moved), "--output", str(incomplete_output)
                ]), 1)
            self.assertFalse(incomplete_output.exists())


if __name__ == "__main__":
    unittest.main()
