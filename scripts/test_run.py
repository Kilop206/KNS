import csv
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

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


if __name__ == "__main__":
    unittest.main()
