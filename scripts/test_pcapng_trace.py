from contextlib import redirect_stderr, redirect_stdout
from decimal import Decimal
from fractions import Fraction
import io
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

import experiment_profile
import pcapng_trace
import trace_import
from test_pcap_trace import ADDRESS_MAP, LAB, capture, frame


def option(code, value=b"", endian="<"):
    return struct.pack(endian + "HH", code, len(value)) + value + bytes((-len(value)) % 4)


def block(kind, body, endian="<"):
    assert len(body) % 4 == 0
    size = len(body) + 12
    return struct.pack(endian + "II", kind, size) + body + struct.pack(endian + "I", size)


def section(endian="<", length=-1):
    return block(0x0A0D0D0A, struct.pack(endian + "IHHq", 0x1A2B3C4D, 1, 0, length) +
        option(1, b"PRIVATE-CAPTURE-HOST", endian) + option(0, endian=endian), endian)


def interface(endian="<", tsresol=None, time_offset=None, snaplen=4096, extra=b"", linktype=1):
    values = option(2, b"PRIVATE-CAPTURE-INTERFACE", endian)
    if tsresol is not None: values += option(9, bytes([tsresol]), endian)
    if time_offset is not None: values += option(14, struct.pack(endian + "q", time_offset), endian)
    return block(1, struct.pack(endian + "HHI", linktype, 123, snaplen) + values + extra + option(0, endian=endian), endian)


def enhanced(ticks, packet, original=None, endian="<", identifier=0, extra=b""):
    header = struct.pack(endian + "IIIII", identifier, ticks >> 32, ticks & 0xffffffff,
        len(packet), len(packet) if original is None else original)
    return block(6, header + packet + bytes((-len(packet)) % 4) + extra, endian)


def capture_ng(endian="<", tsresol=None, time_offset=None, records=None, known_length=False, snaplen=4096):
    base, exponent = (2 if tsresol is not None and tsresol & 128 else 10), (6 if tsresol is None else tsresol & 127)
    scale = base ** exponent
    if records is None:
        origin = 1700000000 * scale
        records = [(origin, frame(), None), (origin, frame(), None), (origin, frame(6), None),
                   (origin + scale // 100, frame()[:42], len(frame()))]
    body = interface(endian, tsresol, time_offset, snaplen) + b"".join(enhanced(t, p, o, endian) for t,p,o in records)
    return section(endian, len(body) if known_length else -1) + body


class PcapngTraceTests(unittest.TestCase):
    def setUp(self):
        self.topology = json.loads((LAB / "topology.json").read_text())

    def test_byte_order_default_and_decimal_resolution_match_classic_replay(self):
        classic, _ = trace_import.convert(capture(), "pcap", self.topology, address_map=ADDRESS_MAP)
        for endian in ("<", ">"):
            for resolution in (None, 6, 9):
                for known in (False, True):
                    data = capture_ng(endian, resolution, time_offset=-12345, known_length=known)
                    plan, metadata = trace_import.convert(data, "pcapng", self.topology, address_map=ADDRESS_MAP)
                    self.assertEqual(plan, classic)
                    ng = metadata["pcapng"]
                    self.assertEqual(ng["block_counts"], {"section": 1, "interface": 1, "enhanced_packet": 4})
                    self.assertEqual(ng["timestamp_resolution"], {"base": 10, "exponent": resolution or 6})
                    self.assertEqual(ng["truncated_records"], 1)
                    self.assertTrue(ng["timestamp_offset_applied"])
                    self.assertEqual(ng["discarded_option_count"], 2)
                    self.assertNotIn("PRIVATE-CAPTURE", json.dumps((plan, metadata)))
        rows, _ = pcapng_trace.parse(capture_ng(records=[(2000000, frame(), None)], time_offset=-1), ADDRESS_MAP, 2)
        self.assertEqual(rows[0]["time_s"], Decimal(1))

    def test_binary_and_extreme_timestamp_resolution_remain_ordered(self):
        for endian in ("<", ">"):
            data = capture_ng(endian, 128 + 10, records=[(2**40, frame(), None), (2**40 + 16, frame(), None)], snaplen=0)
            plan, meta = trace_import.convert(data, "pcapng", self.topology, address_map=ADDRESS_MAP)
            self.assertEqual(plan["replay_events"][1]["at_s"], 0.015625)
            self.assertEqual(meta["pcapng"]["timestamp_resolution"], {"base": 2, "exponent": 10})
        for resolution, expected in ((127, 1e-127), (255, float(Decimal(1) / (2**127)))):
            plan, _ = trace_import.convert(capture_ng(tsresol=resolution, records=[(1, frame(), None), (2, frame(), None)]),
                "pcapng", self.topology, address_map=ADDRESS_MAP)
            self.assertEqual(plan["replay_events"][1]["at_s"], expected)

    def test_binary_midpoints_are_rounded_once_to_the_virtual_clock(self):
        ticks = 2**53 + 1
        for endian in ("<", ">"):
            data = capture_ng(endian, 128 + 64, records=[(0, frame(), None), (ticks, frame(), None)])
            plan, metadata = trace_import.convert(data, "pcapng", self.topology, address_map=ADDRESS_MAP)
            self.assertEqual(plan["replay_events"][1]["at_s"], float(Fraction(ticks, 2**64)))
            self.assertEqual(metadata["decimal_normalization_precision"], 200)

    def test_malformed_or_unsupported_containers_fail_without_skipping_packets(self):
        good = capture_ng()
        invalid = [b"", good[:27], good[:-1], good + b"x", section(), section() + interface() + block(3, bytes(4)),
            section() + interface(linktype=101), section() + interface(snaplen=1048577),
            section() + interface() + interface(), good + section(),
            section() + enhanced(0, frame()), section() + interface() + enhanced(0, frame(), identifier=1),
            section() + interface(snaplen=41) + enhanced(0, frame()),
            section() + interface() + enhanced(0, frame(), original=1),
            section() + interface() + block(6, bytes(16)),
            section() + interface() + block(0xFFFFFFFF, b""),
            section() + interface() + enhanced(0, frame()[:41], original=len(frame()))]
        for position, value, code in ((4, 29, "I"), (8, 0, "I"), (12, 2, "H"), (16, -2, "q"), (16, len(good), "q")):
            bad = bytearray(good); struct.pack_into("<" + code, bad, position, value); invalid.append(bytes(bad))
        bad = bytearray(good); bad[-4:] = bytes(4); invalid.append(bytes(bad))
        for data in invalid:
            with self.subTest(prefix=data[:24]), self.assertRaises(ValueError):
                trace_import.convert(data, "pcapng", self.topology, address_map=ADDRESS_MAP)
        with self.assertRaisesRegex(ValueError, "ordered"):
            trace_import.convert(capture_ng(records=[(2, frame(), None), (1, frame(), None)]), "pcapng", self.topology, address_map=ADDRESS_MAP)
        with self.assertRaisesRegex(ValueError, "4 MiB"):
            pcapng_trace.parse(b"x" * (4 * 1024 * 1024 + 1), ADDRESS_MAP, 2)
        with self.assertRaisesRegex(ValueError, "10000"):
            pcapng_trace.parse(capture_ng(records=[(0, frame(), None)] * 10001), ADDRESS_MAP, 2)

    def test_metadata_option_count_is_bounded(self):
        data = section() + interface(extra=option(1) * 4096) + enhanced(0, frame())
        with self.assertRaisesRegex(ValueError, "4096 options"):
            pcapng_trace.parse(data, ADDRESS_MAP, 2)

    def test_clock_options_and_truncated_option_bounds_are_validated(self):
        bad_options = [option(9, b"xx"), option(14, bytes(4)), option(9, b"\x06") * 2,
            option(14, bytes(8)) * 2, struct.pack("<HH", 9, 100), option(0, b"x"), option(0) + option(1, b"after-end")]
        for extra in bad_options:
            data = section() + interface(extra=extra) + enhanced(0, frame())
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                pcapng_trace.parse(data, ADDRESS_MAP, 2)
        data = section() + interface() + enhanced(0, frame(), extra=struct.pack("<HH", 1, 100))
        with self.assertRaisesRegex(ValueError, "option length"):
            pcapng_trace.parse(data, ADDRESS_MAP, 2)
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); source = root / "bad.pcapng"; source.write_bytes(data); output = root / "output"
            mapping = root / "map.json"; mapping.write_text(json.dumps(ADDRESS_MAP))
            with redirect_stderr(io.StringIO()):
                self.assertEqual(trace_import.main([str(source), "--topology", str(LAB / "topology.json"), "--address-map", str(mapping), "--output", str(output)]), 1)
            self.assertFalse(output.exists())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for actual PCAPNG replay")
    def test_actual_replay_and_manifest_remain_usable_without_source_capture(self):
        with tempfile.TemporaryDirectory(prefix="KNS pcapng ") as folder:
            root = Path(folder); source = root / "private.pcapng"; source.write_bytes(capture_ng())
            mapping = root / "map.json"; mapping.write_text(json.dumps(ADDRESS_MAP)); output = root / "imported"
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(trace_import.main([str(source), "--topology", str(LAB / "topology.json"), "--address-map", str(mapping),
                    "--output", str(output), "--exe", os.environ["KNS_TEST_EXE"]]), 0)
            for path in output.glob("*.json"):
                self.assertNotIn("PRIVATE-CAPTURE", path.read_text())
                self.assertNotIn("192.0.2.10", path.read_text())
            runs = output / "experiment/runs"
            report = json.loads(next(runs.glob("*.traffic.json")).read_text())
            self.assertEqual((report["generated"], report["delivered"], report["dropped"]), (4, 3, 1))
            self.assertEqual(sum(f["delivered_wire_bytes"] for f in report["flows"]), 3 * len(frame()))
            source.unlink(); mapping.unlink(); replay = root / "replay"
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(experiment_profile.main(["--replay", str(output / "experiment/manifest.json"), "--output", str(replay), "--exe", os.environ["KNS_TEST_EXE"]]), 0)
            for path in runs.glob("*.csv.traffic.json"):
                self.assertEqual(path.read_bytes(), (replay / "runs" / path.name).read_bytes())
                name = path.name.removesuffix(".traffic.json")
                self.assertEqual((runs / name).read_bytes(), (replay / "runs" / name).read_bytes())


if __name__ == "__main__":
    unittest.main()
