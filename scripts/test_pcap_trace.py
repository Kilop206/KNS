from contextlib import redirect_stderr, redirect_stdout
import copy
import io
import ipaddress
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

import pcap_trace
import trace_import

ROOT = Path(__file__).resolve().parent.parent
LAB = ROOT / "KNS Experiments/trace-lab"
ADDRESS_MAP = {"schema_version": "1.0", "ipv4_to_node": {"192.0.2.10": 0, "192.0.2.20": 1}}


def frame(protocol=17, payload=b"PRIVATE-CAPTURE-DATA"):
    if protocol == 17:
        transport = struct.pack("!HHHH", 1234, 4321, 8 + len(payload), 0)
    else:
        transport = struct.pack("!HHIIBBHHH", 1234, 4321, 0, 0, 0x50, 0x18, 4096, 0, 0)
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(transport) + len(payload), 0, 0x4000,
        64, protocol, 0, ipaddress.IPv4Address("192.0.2.10").packed, ipaddress.IPv4Address("192.0.2.20").packed)
    return bytes(12) + b"\x08\x00" + ip + transport + payload


def capture(endian="<", nanos=False, records=None, linktype=1, snaplen=4096):
    magic = b"\x4d\x3c\xb2\xa1" if nanos else b"\xd4\xc3\xb2\xa1"
    if endian == ">": magic = magic[::-1]
    # Nonzero reserved fields are deliberately ignored, per libpcap format.
    result = magic + struct.pack(endian + "HHIIII", 2, 4, 123, 456, snaplen, linktype)
    if records is None:
        records = [(0, frame(), None), (0, frame(), None), (0, frame(6), None),
                   (10000000 if nanos else 10000, frame()[:42], len(frame()))]
    for fraction, packet, original in records:
        result += struct.pack(endian + "IIII", 1700000000, fraction, len(packet), len(packet) if original is None else original) + packet
    return result


class PcapTraceTests(unittest.TestCase):
    def setUp(self):
        self.topology = json.loads((LAB / "topology.json").read_text())

    def test_endianness_and_timestamp_resolution_preserve_the_same_timeline(self):
        plans = []
        for endian in ("<", ">"):
            for nanos in (False, True):
                plan, metadata = trace_import.convert(capture(endian, nanos), "pcap", self.topology, address_map=ADDRESS_MAP)
                self.assertEqual([event["at_s"] for event in plan["replay_events"]], [0, 0, 0, 0.01])
                self.assertEqual(metadata["pcap"]["protocol_counts"], {"udp": 3, "tcp": 1})
                self.assertEqual(metadata["pcap"]["truncated_records"], 1)
                self.assertEqual(metadata["wire_bytes_total"], 3 * len(frame()) + len(frame(6)))
                self.assertNotIn("192.0.2.10", json.dumps((plan, metadata)))
                self.assertNotIn("PRIVATE-CAPTURE-DATA", json.dumps((plan, metadata)))
                plans.append(plan)
        self.assertTrue(all(plan == plans[0] for plan in plans))
        plan, _ = trace_import.convert(capture(nanos=True, records=[(1, frame(), None), (2, frame(), None)]), "pcap", self.topology, address_map=ADDRESS_MAP)
        self.assertEqual(plan["replay_events"][1]["at_s"], 1e-9)

    def test_malformed_and_unsupported_inputs_fail_without_partial_import(self):
        good = capture(records=[(0, frame(), None)])
        bad_inputs = [b"", good[:23], good[:30], good[:-1], good + b"x", capture(records=[]),
                      capture(linktype=101), capture(linktype=0x04000001), capture(snaplen=0),
                      capture(records=[(1000000, frame(), None)]), capture(records=[(0, frame()[:41], len(frame()))])]
        for start, value, code in ((24 + 8, 5000, "I"), (24 + 12, 1, "I"), (4, 3, "H")):
            bad = bytearray(good); struct.pack_into("<" + code, bad, start, value); bad_inputs.append(bytes(bad))
        for position, value in ((12, b"\x86\xdd"), (12, b"\x81\x00"), (14, b"\x65"),
                                (14 + 2, b"\x00\x01"), (14 + 6, b"\x20\x00"), (14 + 9, b"\x01"),
                                (14 + 20 + 4, b"\x00\x01")):
            packet = bytearray(frame()); packet[position:position + len(value)] = value
            bad_inputs.append(capture(records=[(0, bytes(packet), None)]))
        tcp = bytearray(frame(6)); tcp[14 + 20 + 12] = 0xF0
        bad_inputs.append(capture(records=[(0, bytes(tcp), None)]))
        for data in bad_inputs:
            with self.assertRaises(ValueError): pcap_trace.parse(data, ADDRESS_MAP, 2)
        unknown = copy.deepcopy(ADDRESS_MAP); del unknown["ipv4_to_node"]["192.0.2.20"]
        with self.assertRaises(ValueError): pcap_trace.parse(good, unknown, 2)
        invalid = copy.deepcopy(ADDRESS_MAP); invalid["ipv4_to_node"]["192.0.2.20"] = True
        with self.assertRaises(ValueError): pcap_trace.parse(good, invalid, 2)

    def test_cli_requires_explicit_mapping_and_preserves_existing_evidence(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); source = root / "capture.pcap"; source.write_bytes(capture())
            output = root / "import"
            with redirect_stderr(io.StringIO()):
                self.assertEqual(trace_import.main([str(source), "--topology", str(LAB / "topology.json"), "--output", str(output)]), 1)
            self.assertFalse(output.exists())
            self.assertEqual(source.read_bytes(), capture())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for PCAP replay integration")
    def test_actual_classic_pcap_replay_uses_orig_lengths_and_real_queue_drops(self):
        with tempfile.TemporaryDirectory(prefix="KNS pcap ") as folder:
            root = Path(folder); source = root / "private.pcap"; source.write_bytes(capture())
            mapping = root / "map.json"; mapping.write_text(json.dumps(ADDRESS_MAP))
            output = root / "imported"
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(trace_import.main([str(source), "--topology", str(LAB / "topology.json"), "--address-map", str(mapping),
                    "--output", str(output), "--exe", os.environ["KNS_TEST_EXE"]]), 0)
            metadata = json.loads((output / "trace-metadata.json").read_text())
            self.assertEqual(metadata["pcap"]["record_count"], 4)
            self.assertEqual(metadata["pcap"]["protocol_counts"], {"tcp": 1, "udp": 3})
            for path in output.glob("*.json"):
                self.assertNotIn("PRIVATE-CAPTURE-DATA", path.read_text())
                self.assertNotIn("192.0.2.10", path.read_text())
            report_path = next((output / "experiment/runs").glob("*.traffic.json"))
            report = json.loads(report_path.read_text())
            self.assertEqual((report["generated"], report["delivered"], report["dropped"]), (4, 3, 1))
            self.assertEqual(sum(flow["delivered_wire_bytes"] for flow in report["flows"]), 3 * len(frame()))
            self.assertTrue(all(flow["config"]["schedule"]["kind"] == "replay" for flow in report["flows"]))


if __name__ == "__main__":
    unittest.main()
