import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from compare_operator_plans import ROOT, compare, main


class CompareTreeTests(unittest.TestCase):
    def test_objects_ignore_order_but_arrays_keep_order(self):
        result = compare({"b": [1, 2], "a": 1}, {"a": 1.0, "b": [1.0, 2]})
        self.assertEqual(result["status"], "equivalent")
        self.assertEqual(result["changes"], [])
        changed = compare([1, 2], [2, 1])
        self.assertEqual([change["path"] for change in changed["changes"]], ["/0", "/1"])

    def test_missing_null_booleans_and_exact_large_integers(self):
        result = compare({"bool": True, "removed": None, "id": 9007199254740992},
                         {"bool": 1, "added": None, "id": 9007199254740993})
        self.assertEqual(result["change_count"], 4)
        self.assertEqual(result["changes"][0], {"kind": "added", "path": "/added", "after": None})
        self.assertEqual(result["changes"][-1], {"kind": "removed", "path": "/removed", "before": None})
        self.assertEqual(result["changes"][2]["after"], 9007199254740993)
        self.assertEqual(compare(1, True)["change_count"], 1)
        self.assertEqual(compare(9007199254740993, float(9007199254740993))["change_count"], 1)

    def test_pointer_escaping_and_container_replacement(self):
        result = compare({"a/b~c": {"": [1]}}, {"a/b~c": {"": [2, None]}})
        self.assertEqual(result["changes"], [
            {"kind": "changed", "path": "/a~1b~0c//0", "before": 1, "after": 2},
            {"kind": "added", "path": "/a~1b~0c//1", "after": None},
        ])
        self.assertEqual(compare({}, [])["changes"], [
            {"kind": "changed", "path": "", "before": {}, "after": []},
        ])

    def test_truncation_counts_every_difference_in_stable_order(self):
        result = compare({"z": 0, "b": 0, "a": 0}, {"a": 1, "b": 1, "z": 1}, 2)
        self.assertEqual(result["change_count"], 3)
        self.assertEqual(result["reported_changes"], 2)
        self.assertTrue(result["truncated"])
        self.assertEqual([item["path"] for item in result["changes"]], ["/a", "/b"])
        for maximum in (0, 1001, True, 1.5):
            with self.assertRaises(ValueError):
                compare({}, {}, maximum)

    def test_syntax_errors_do_not_reuse_difference_exit_code(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as exit_status:
            main([])
        self.assertEqual(exit_status.exception.code, 1)

    def test_long_ancestor_keys_cannot_multiply_report_size_without_bound(self):
        key = "long-ancestor-" * 10
        with patch("compare_operator_plans.MAX_REPORTED_BYTES", 512):
            report = compare({key: [0] * 32}, {key: [1] * 32}, 1000)
        self.assertEqual(report["change_count"], 32)
        self.assertGreater(report["reported_changes"], 0)
        self.assertLess(report["reported_changes"], 32)
        self.assertTrue(report["truncated"])
        self.assertEqual(report["changes"][0]["path"], "/" + key + "/0")


@unittest.skipUnless(importlib.util.find_spec("jsonschema"), "contract dependencies not installed")
class CompareCliTests(unittest.TestCase):
    def setUp(self):
        from jsonschema import Draft202012Validator
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.reference = self.directory / "reference.json"
        self.candidate = self.directory / "candidate.json"
        self.output = self.directory / "report"
        self.plan = json.loads((ROOT / "contracts/fixtures/operator-plan-v1.valid.https.json").read_text())
        self.validator = Draft202012Validator(json.loads(
            (ROOT / "contracts/operator-plan-comparison-v1.schema.json").read_text()))
        self.reference.write_text(json.dumps(self.plan), encoding="utf-8")
        self.candidate.write_text(json.dumps(self.plan, sort_keys=True, indent=2), encoding="utf-8")

    def invoke(self, *extra):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = main([str(self.reference), str(self.candidate), "--output", str(self.output), *extra])
        return code, stdout.getvalue(), stderr.getvalue()

    def report(self):
        report = json.loads((self.output / "comparison.json").read_text())
        self.validator.validate(report)
        self.assertEqual(report["reported_changes"], len(report["changes"]))
        return report

    def test_equivalent_files_have_distinct_byte_hashes_and_cannot_be_overwritten(self):
        self.assertEqual(self.invoke()[0], 0)
        report = self.report()
        self.assertEqual(report["status"], "equivalent")
        self.assertEqual(report["reference_sha256"], hashlib.sha256(self.reference.read_bytes()).hexdigest())
        self.assertNotEqual(report["reference_sha256"], report["candidate_sha256"])
        original = (self.output / "comparison.json").read_bytes()
        self.assertEqual(self.invoke()[0], 1)
        self.assertEqual((self.output / "comparison.json").read_bytes(), original)

    def test_difference_exit_code_full_count_and_report_truncation(self):
        changed = copy.deepcopy(self.plan)
        changed["timeout_seconds"] = 60
        changed["device"]["id"] = "different"
        self.candidate.write_text(json.dumps(changed))
        self.assertEqual(self.invoke("--max-changes", "1")[0], 2)
        report = self.report()
        self.assertEqual(report["change_count"], 2)
        self.assertEqual(report["reported_changes"], 1)
        self.assertTrue(report["truncated"])

    def test_missing_dependencies_fail_cleanly_before_publication(self):
        original_import = __import__

        def unavailable(name, *args, **kwargs):
            if name == "jsonschema":
                raise ImportError("test-only missing dependency")
            return original_import(name, *args, **kwargs)

        with patch("builtins.__import__", side_effect=unavailable):
            code, _, stderr = self.invoke()
        self.assertEqual(code, 1)
        self.assertIn("requirements-contracts.txt", stderr)
        self.assertFalse(self.output.exists())

    def test_invalid_inputs_do_not_publish_or_expose_plan_values(self):
        invalid = copy.deepcopy(self.plan)
        invalid["timeout_seconds"] = "private-body-content"
        inputs = [json.dumps(invalid), "not json private-body-content", '{"a":1,"a":2}',
                  '{"a":1e999}', "[" * 65 + "0" + "]" * 65,
                  " " * (4 * 1024 * 1024 + 1)]
        for source in inputs:
            with self.subTest(length=len(source)):
                self.candidate.write_text(source)
                code, stdout, stderr = self.invoke()
                self.assertEqual(code, 1)
                self.assertIn("candidate", stderr)
                self.assertNotIn("private-body-content", stdout + stderr)
                self.assertFalse(self.output.exists())
        self.reference.write_text("null")
        self.assertIn("reference", self.invoke()[2])

    def test_html_escapes_untrusted_bodies_and_preserves_exact_integer_ids(self):
        plan = copy.deepcopy(self.plan)
        plan.update(action="service.add", read_only=False, requires_apply=True)
        plan["steps"][0].update(method="POST", body={"id": 9007199254740992})
        self.reference.write_text(json.dumps(plan))
        plan["steps"][0]["body"].update(id=9007199254740993,
                                         text='</pre><script src="https://example.invalid/evil"></script>')
        self.candidate.write_text(json.dumps(plan))
        self.assertEqual(self.invoke()[0], 2)
        self.assertEqual(self.report()["changes"][0]["after"], 9007199254740993)
        page = (self.output / "report.html").read_text(encoding="utf-8")
        self.assertIn("&lt;script", page)
        self.assertNotIn("<script", page)
        self.assertIn("default-src 'none'", page)


if __name__ == "__main__":
    unittest.main()
