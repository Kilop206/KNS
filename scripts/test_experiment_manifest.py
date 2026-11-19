import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import experiment_manifest as manifests


class ManifestProvenanceTests(unittest.TestCase):
    def test_artifact_integrity_rejects_incomplete_maps_invalid_hashes_and_missing_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "runs").mkdir()
            csv = root / "runs/case.csv"
            log = root / "runs/case.log"
            csv.write_bytes(b"schema_version,packets_sent\n1,10\n")
            log.write_bytes(b"complete\n")
            hashes = {"csv": manifests.file_hash(csv), "log": manifests.file_hash(log)}
            case = {
                "case_id": "case", "csv": "Z:\\old\\case.csv",
                "log": "/old/case.log", "artifact_sha256": hashes,
            }
            self.assertEqual(manifests.verify_case_artifacts(root, [case]), {
                "hashed_cases": 1, "legacy_cases": 0, "verified_artifacts": 2,
            })
            for invalid in (
                None, {}, {"csv": hashes["csv"]},
                {**hashes, "unknown": hashes["csv"]},
                {**hashes, "csv": True},
                {**hashes, "csv": "0" * 64},
            ):
                with self.subTest(hashes=invalid), self.assertRaises(ValueError):
                    manifests.verify_case_artifacts(root, [{**case, "artifact_sha256": invalid}])
            csv.unlink()
            with self.assertRaises(OSError):
                manifests.verify_case_artifacts(root, [case])

    def test_source_version_is_explicitly_unknown_without_build_sources(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(manifests.source_version(Path(folder)), "unknown")

    def test_source_version_reads_only_the_declared_project_version(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "CMakeLists.txt").write_text(
                "project(KNS VERSION 1.2.3 LANGUAGES CXX)"
            )
            self.assertEqual(manifests.source_version(root), "1.2.3")
            (root / "CMakeLists.txt").write_text("project(another VERSION 9.9)")
            self.assertEqual(manifests.source_version(root), "unknown")

    def test_source_version_does_not_mask_binary_identity_or_modify_inputs(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            exe = root / "test-binary"
            exe.write_bytes(b"unit-test executable identity fixture")
            binary_hash = manifests.file_hash(exe)
            with patch.object(manifests, "file_hash") as hashing:
                # Runner files are separate provenance; emulate their presence
                # without requiring this test fixture to copy the source tree.
                hashing.side_effect = lambda p: binary_hash if p == exe else "0" * 64
                result = manifests.create({"topology": "input.json"}, {}, 1, exe, root)
                with self.assertRaisesRegex(ValueError, "SHA-256 differs"):
                    manifests.create(
                        {"topology": "input.json"},
                        {},
                        1,
                        exe,
                        root,
                        previous={"executable_sha256": "1" * 64},
                    )
            self.assertEqual(result["source_version"], "unknown")
            self.assertEqual(result["executable_sha256"], binary_hash)
            self.assertFalse((root / "CMakeLists.txt").exists())

    def test_unreadable_version_source_is_not_a_guessed_version(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "CMakeLists.txt").write_bytes(b"\xff\xfe")
            self.assertEqual(manifests.source_version(root), "unknown")


if __name__ == "__main__":
    unittest.main()
