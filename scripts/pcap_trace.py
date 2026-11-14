"""Bounded PCAP 2.4 Ethernet/IPv4 TCP/UDP metadata reader; no capture or sockets.

Format references and intentionally unsupported variants are in trace-import.md.
This is a strict local subset reader, not a libpcap-compatible general dissector.
"""
from decimal import Decimal
import hashlib
import ipaddress
import json
from pathlib import Path
import struct

MAGICS = {b"\xd4\xc3\xb2\xa1": ("<", 1000000), b"\xa1\xb2\xc3\xd4": (">", 1000000),
          b"\x4d\x3c\xb2\xa1": ("<", 1000000000), b"\xa1\xb2\x3c\x4d": (">", 1000000000)}


def ethernet_payload(frame):
    """Locate L3 after at most two standard C/S tags; no VLAN behavior inferred."""
    if len(frame) < 14:
        raise ValueError("PCAP record has a truncated Ethernet header")
    ether_type = struct.unpack_from("!H", frame, 12)[0]
    offset, tags = 14, []
    while ether_type in (0x8100, 0x88A8):
        if len(tags) == 2:
            raise ValueError("PCAP VLAN stacks deeper than two tags are unsupported")
        if len(frame) < offset + 4:
            raise ValueError("PCAP record has a truncated VLAN tag/inner EtherType")
        control, inner_type = struct.unpack_from("!HH", frame, offset)
        tags.append(f"{ether_type:04x}:{control & 0xfff}")
        ether_type = inner_type
        offset += 4
    return ether_type, offset, tags


def mapping(value, node_count):
    if not isinstance(value, dict) or set(value) != {"schema_version", "ipv4_to_node"} or value["schema_version"] != "1.0":
        raise ValueError("PCAP requires a version-1.0 ipv4_to_node map")
    addresses = value["ipv4_to_node"]
    if not isinstance(addresses, dict) or not 1 <= len(addresses) <= 4096:
        raise ValueError("Address map requires 1..4096 explicit IPv4 entries")
    for address, node in addresses.items():
        if not isinstance(address, str) or str(ipaddress.IPv4Address(address)) != address:
            raise ValueError("Address map requires canonical IPv4 addresses")
        if type(node) is not int or not 0 <= node < node_count:
            raise ValueError("Address map selects an unknown model node")
    return addresses


def parse(data, address_map, node_count):
    if len(data) > 4 * 1024 * 1024:
        raise ValueError("PCAP input exceeds 4 MiB")
    addresses = mapping(address_map, node_count)
    if len(data) < 24:
        raise ValueError("Truncated PCAP file header")
    if data[:4] not in MAGICS:
        raise ValueError("Unsupported PCAP magic; PCAPNG and modified formats are not implemented")
    endian, resolution = MAGICS[data[:4]]
    major, minor, _, _, snaplen, linktype = struct.unpack_from(endian + "HHIIII", data, 4)
    if (major, minor) != (2, 4) or not 1 <= snaplen <= 1048576 or linktype != 1:
        raise ValueError("PCAP requires version 2.4, bounded snaplen and plain Ethernet link type 1 without additional flags")
    rows, protocols, offset, truncated = [], {"tcp": 0, "udp": 0}, 24, 0
    vlan_records = vlan_depth = 0
    while offset < len(data):
        if len(rows) >= 10000:
            raise ValueError("PCAP exceeds 10000 records")
        if len(data) - offset < 16:
            raise ValueError("Truncated PCAP record header")
        seconds, fraction, captured, original = struct.unpack_from(endian + "IIII", data, offset)
        offset += 16
        if fraction >= resolution or not 1 <= captured <= min(snaplen, original) or not 1 <= original <= 1048576:
            raise ValueError("PCAP record has invalid timestamp/length fields")
        if captured > len(data) - offset:
            raise ValueError("PCAP record extends beyond input")
        frame = data[offset:offset + captured]; offset += captured
        ether_type, ip, tags = ethernet_payload(frame)
        if ether_type != 0x0800:
            raise ValueError("PCAP requires Ethernet IPv4; other media/IPv6 are unsupported")
        if len(frame) < ip + 20 or frame[ip] >> 4 != 4:
            raise ValueError("PCAP record has a truncated/invalid IPv4 header")
        header_size = (frame[ip] & 15) * 4
        total_size = struct.unpack_from("!H", frame, ip + 2)[0]
        fragment = struct.unpack_from("!H", frame, ip + 6)[0]
        if header_size < 20 or len(frame) < ip + header_size or total_size < header_size or original < ip + total_size:
            raise ValueError("PCAP IPv4 lengths contradict captured/original bounds")
        if fragment & ~0x4000:
            raise ValueError("Fragmented IPv4/reserved fragment flags are unsupported; no reassembly is inferred")
        source = str(ipaddress.IPv4Address(frame[ip + 12:ip + 16]))
        destination = str(ipaddress.IPv4Address(frame[ip + 16:ip + 20]))
        if source not in addresses or destination not in addresses:
            raise ValueError("PCAP endpoint is not explicitly mapped to a model node")
        protocol = frame[ip + 9]
        transport, transport_size = ip + header_size, total_size - header_size
        if protocol == 17:
            if transport_size < 8 or len(frame) < transport + 8:
                raise ValueError("PCAP UDP header is truncated")
            udp_size = struct.unpack_from("!H", frame, transport + 4)[0]
            if udp_size != transport_size or udp_size < 8:
                raise ValueError("PCAP UDP length contradicts IPv4 payload")
            kind = "udp"
        elif protocol == 6:
            if transport_size < 20 or len(frame) < transport + 20:
                raise ValueError("PCAP TCP header is truncated")
            tcp_size = (frame[transport + 12] >> 4) * 4
            if tcp_size < 20 or tcp_size > transport_size or len(frame) < transport + tcp_size:
                raise ValueError("PCAP TCP data offset exceeds header/payload bounds")
            kind = "tcp"
        else:
            raise ValueError("PCAP IPv4 protocol is unsupported; only TCP/UDP metadata is imported")
        source_port, destination_port = struct.unpack_from("!HH", frame, transport)
        rows.append({"time_s": Decimal(seconds) + Decimal(fraction) / resolution,
            "flow_id": f"{source}:{source_port}>{destination}:{destination_port}/{kind}" + (";vlan=" + ",".join(tags) if tags else ""),
            "source": addresses[source], "destination": addresses[destination], "wire_bytes": original})
        protocols[kind] += 1
        truncated += captured < original
        vlan_records += bool(tags)
        vlan_depth = max(vlan_depth, len(tags))
    if not rows:
        raise ValueError("PCAP contains no packet records")
    canonical_map = json.dumps(address_map, sort_keys=True, separators=(",", ":")).encode()
    return rows, {"version": "2.4", "link_type": "ethernet_ipv4", "byte_order": "little" if endian == "<" else "big",
        "timestamp_resolution": "microseconds" if resolution == 1000000 else "nanoseconds",
        "record_count": len(rows), "protocol_counts": protocols, "truncated_records": truncated,
        "vlan_tagged_records": vlan_records, "max_vlan_depth": vlan_depth,
        "address_map_sha256": hashlib.sha256(canonical_map).hexdigest(),
        "parser_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "wire_bytes_scope": "PCAP original record length including recorded link header; no added preamble/interframe gap/FCS",
        "limitations": "Structural header checks only; checksums and payload are not validated/exported; no TCP/UDP protocol state, fragmentation replay or VLAN forwarding/priority semantics"}
