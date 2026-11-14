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
DUAL_MAP = {"schema_version": "1.1", "ipv4_to_node": ADDRESS_MAP["ipv4_to_node"],
            "ipv6_to_node": {"2001:db8::10": 0, "2001:db8::20": 1}}


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


def tagged(packet, tags):
    return packet[:12] + b"".join(struct.pack("!HH", tpid, control) for tpid, control in tags) + packet[12:]


def frame6(protocol=17):
    transport_payload = frame(protocol)[34:]
    header = struct.pack("!IHBB16s16s", 6 << 28, len(transport_payload), protocol, 64,
        ipaddress.IPv6Address("2001:db8::10").packed, ipaddress.IPv6Address("2001:db8::20").packed)
    return bytes(12) + b"\x86\xdd" + header + transport_payload


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

    def test_vlan_tags_preserve_flow_separation_and_original_record_bytes(self):
        packets = [frame(), tagged(frame(), [(0x8100, 37)]),
            tagged(frame(), [(0x8100, 37 | 0xF000)]), tagged(frame(), [(0x8100, 38)]),
            tagged(frame(), [(0x88A8, 37)]), tagged(frame(), [(0x88A8, 12), (0x8100, 37)])]
        data = capture(records=[(index, packet, None) for index, packet in enumerate(packets)])
        plan, metadata = trace_import.convert(data, "pcap", self.topology, address_map=ADDRESS_MAP)
        # PCP/DEI changes do not split a flow or create modeled priority.
        self.assertEqual([event["flow_id"] for event in plan["replay_events"]], [1, 2, 2, 3, 4, 5])
        self.assertEqual(metadata["source_flow_count"], 5)
        self.assertEqual(metadata["wire_bytes_total"], sum(map(len, packets)))
        self.assertEqual((metadata["pcap"]["vlan_tagged_records"], metadata["pcap"]["max_vlan_depth"]), (5, 2))
        self.assertNotIn(";vlan=", json.dumps((plan, metadata)))
        _, labelled = trace_import.convert(data, "pcap", self.topology, keep_flow_labels=True, address_map=ADDRESS_MAP)
        self.assertTrue(labelled["wire_groups"][-1]["source_flow_label"].endswith(";vlan=88a8:12,8100:37"))
        for count in (1, 2):
            packet = tagged(frame(), [(0x8100, 37)] * count)
            data = capture(records=[(0, packet[:42 + 4 * count], len(packet))])
            _, truncated = trace_import.convert(data, "pcap", self.topology, address_map=ADDRESS_MAP)
            self.assertEqual(truncated["pcap"]["truncated_records"], 1)

    def test_incomplete_or_excess_vlan_stacks_are_rejected(self):
        for size in range(14, 18):
            with self.subTest(size=size), self.assertRaisesRegex(ValueError, "truncated VLAN"):
                pcap_trace.parse(capture(records=[(0, tagged(frame(), [(0x8100, 37)])[:size], None)]), ADDRESS_MAP, 2)
        excessive = tagged(frame(), [(0x88A8, 12), (0x8100, 37), (0x8100, 38)])
        with self.assertRaisesRegex(ValueError, "deeper than two"):
            pcap_trace.parse(capture(records=[(0, excessive, None)]), ADDRESS_MAP, 2)

    def test_ipv6_and_mixed_ip_records_preserve_explicit_mapping_and_privacy(self):
        packets = [frame(), frame6(), frame6(6), tagged(frame6(), [(0x88A8, 12), (0x8100, 37)])]
        data = capture(records=[(index, packet, None) for index, packet in enumerate(packets)])
        plan, metadata = trace_import.convert(data, "pcap", self.topology, address_map=DUAL_MAP)
        self.assertEqual(metadata["pcap"]["ip_version_counts"], {"ipv4": 1, "ipv6": 3})
        self.assertEqual(metadata["pcap"]["protocol_counts"], {"udp": 3, "tcp": 1})
        self.assertEqual(metadata["wire_bytes_total"], sum(map(len, packets)))
        self.assertEqual(metadata["source_flow_count"], 4)
        self.assertNotIn("2001:db8", json.dumps((plan, metadata)))
        _, labelled = trace_import.convert(data, "pcap", self.topology, keep_flow_labels=True, address_map=DUAL_MAP)
        self.assertTrue(labelled["wire_groups"][1]["source_flow_label"].startswith("[2001:db8::10]:1234>"))
        ipv6_only = {"schema_version": "1.1", "ipv6_to_node": DUAL_MAP["ipv6_to_node"]}
        for protocol, complete_header in ((17, 62), (6, 74)):
            packet = frame6(protocol)
            _, truncated = trace_import.convert(capture(records=[(0, packet[:complete_header], len(packet))]),
                "pcap", self.topology, address_map=ipv6_only)
            self.assertEqual(truncated["pcap"]["truncated_records"], 1)
        self.assertEqual(pcap_trace.mapping(ADDRESS_MAP, 2), ADDRESS_MAP["ipv4_to_node"])

    def test_invalid_ipv6_headers_extensions_and_maps_are_rejected(self):
        bad_packets = [frame6()[:53], frame6()[:61]]
        for position, value in ((14, b"\x40"), (18, b"\x00\x00"), (18, b"\xff\xff"), (58, b"\x00\x01")):
            packet = bytearray(frame6()); packet[position:position + len(value)] = value; bad_packets.append(bytes(packet))
        for next_header in (0, 43, 44, 50, 51, 59, 60, 58):
            packet = bytearray(frame6()); packet[20] = next_header; bad_packets.append(bytes(packet))
        tcp = bytearray(frame6(6)); tcp[66] = 0xF0; bad_packets.append(bytes(tcp))
        for packet in bad_packets:
            with self.subTest(packet=packet[:24]), self.assertRaises(ValueError):
                pcap_trace.parse(capture(records=[(0, packet, None)]), DUAL_MAP, 2)
        invalid_maps = [ADDRESS_MAP, {**DUAL_MAP, "schema_version": "1.0"}, {"schema_version": "1.1"},
            {"schema_version": "1.1", "ipv6_to_node": {}}, {**DUAL_MAP, "extra": {}},
            {"schema_version": "1.1", "ipv6_to_node": {"2001:DB8::10": 0}},
            {"schema_version": "1.1", "ipv6_to_node": {"fe80::1%eth0": 0}},
            {"schema_version": "1.1", "ipv6_to_node": {"2001:db8::10": True}},
            {"schema_version": "1.1", "ipv6_to_node": {"2001:db8::10": 2}},
            {"schema_version": "1.1", "ipv6_to_node": {"2001:db8::10": 0}}]
        for address_map in invalid_maps:
            with self.subTest(address_map=address_map), self.assertRaises(ValueError):
                pcap_trace.parse(capture(records=[(0, frame6(), None)]), address_map, 2)

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
    def test_actual_mixed_ip_replay_preserves_flow_and_byte_accounting(self):
        packets = [frame(), frame6(), frame6(6), tagged(frame6(), [(0x88A8, 12), (0x8100, 37)])]
        data = capture(records=[(0 if index < 3 else 10000, packet, None) for index, packet in enumerate(packets)])
        with tempfile.TemporaryDirectory(prefix="KNS dual stack ") as folder:
            root = Path(folder); source = root / "mixed.pcap"; source.write_bytes(data)
            mapping = root / "map.json"; mapping.write_text(json.dumps(DUAL_MAP))
            output = root / "imported"
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(trace_import.main([str(source), "--topology", str(LAB / "topology.json"), "--address-map", str(mapping),
                    "--output", str(output), "--exe", os.environ["KNS_TEST_EXE"]]), 0)
            report = json.loads(next((output / "experiment/runs").glob("*.traffic.json")).read_text())
            self.assertEqual((report["generated"], report["delivered"], report["dropped"]), (4, 3, 1))
            self.assertEqual(sum(flow["delivered_wire_bytes"] for flow in report["flows"]), sum(len(packets[index]) for index in (0, 1, 3)))
            for path in output.glob("*.json"):
                self.assertNotIn("2001:db8", path.read_text())

    @unittest.skipUnless(os.environ.get("KNS_TEST_EXE"), "Set KNS_TEST_EXE for PCAP replay integration")
    def test_actual_vlan_replay_accounts_tag_bytes_under_queue_contention(self):
        packets = [tagged(frame(), [(0x8100, 37)]), tagged(frame(), [(0x8100, 38)]),
            tagged(frame(6), [(0x88A8, 12), (0x8100, 37)]), tagged(frame(), [(0x88A8, 12), (0x8100, 37)])]
        data = capture(records=[(0 if index < 3 else 10000, packet, None) for index, packet in enumerate(packets)])
        with tempfile.TemporaryDirectory(prefix="KNS VLAN ") as folder:
            root = Path(folder); source = root / "tagged.pcap"; source.write_bytes(data)
            mapping = root / "map.json"; mapping.write_text(json.dumps(ADDRESS_MAP))
            output = root / "imported"
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(trace_import.main([str(source), "--topology", str(LAB / "topology.json"), "--address-map", str(mapping),
                    "--output", str(output), "--exe", os.environ["KNS_TEST_EXE"]]), 0)
            report = json.loads(next((output / "experiment/runs").glob("*.traffic.json")).read_text())
            self.assertEqual((report["generated"], report["delivered"], report["dropped"]), (4, 3, 1))
            self.assertEqual(sum(flow["delivered_wire_bytes"] for flow in report["flows"]), sum(len(packets[index]) for index in (0, 1, 3)))

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
