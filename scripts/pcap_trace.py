"""Bounded PCAP 2.4 Ethernet IP TCP/UDP metadata reader; no capture or sockets.

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
    if not isinstance(value, dict):
        raise ValueError("PCAP requires an explicit versioned address map")
    version = value.get("schema_version")
    fields = set(value) - {"schema_version"}
    if not (version == "1.0" and fields == {"ipv4_to_node"} or
            version == "1.1" and fields and fields <= {"ipv4_to_node", "ipv6_to_node"}):
        raise ValueError("PCAP requires a version-1.0 IPv4 map or version-1.1 IPv4/IPv6 map")
    addresses = {}
    for field in sorted(fields):
        entries = value[field]
        if not isinstance(entries, dict) or not 1 <= len(entries) <= 4096:
            raise ValueError("Each address family requires 1..4096 explicit entries")
        address_type = ipaddress.IPv4Address if field == "ipv4_to_node" else ipaddress.IPv6Address
        for address, node in entries.items():
            if not isinstance(address, str) or "%" in address or str(address_type(address)) != address:
                raise ValueError("Address map requires canonical unscoped IP addresses")
            if type(node) is not int or not 0 <= node < node_count:
                raise ValueError("Address map selects an unknown model node")
            addresses[address] = node
    if len(addresses) > 4096:
        raise ValueError("Address map exceeds 4096 total entries")
    return addresses


def ip_payload(frame, original, ether_type, ip):
    """Return endpoints and direct transport bounds, rejecting IP extensions."""
    if ether_type == 0x0800:
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
        return source, destination, frame[ip + 9], ip + header_size, total_size - header_size, "ipv4"
    if ether_type == 0x86DD:
        if len(frame) < ip + 40 or frame[ip] >> 4 != 6:
            raise ValueError("PCAP record has a truncated/invalid IPv6 base header")
        payload_size = struct.unpack_from("!H", frame, ip + 4)[0]
        if payload_size == 0 or original < ip + 40 + payload_size:
            raise ValueError("PCAP IPv6 payload exceeds original bounds or uses unsupported zero/jumbo length")
        if frame[ip + 6] not in (6, 17):
            raise ValueError("PCAP IPv6 requires direct TCP/UDP; extension/fragment/other headers are unsupported")
        source = str(ipaddress.IPv6Address(frame[ip + 8:ip + 24]))
        destination = str(ipaddress.IPv6Address(frame[ip + 24:ip + 40]))
        return source, destination, frame[ip + 6], ip + 40, payload_size, "ipv6"
    raise ValueError("PCAP requires Ethernet IPv4/IPv6; other EtherTypes/media are unsupported")


def decode_records(records, address_map, node_count):
    """Decode bounded Ethernet records from either capture container."""
    addresses = mapping(address_map, node_count)
    rows, protocols = [], {"tcp": 0, "udp": 0}
    truncated = vlan_records = vlan_depth = 0
    ip_versions = {"ipv4": 0, "ipv6": 0}
    for timestamp, frame, original in records:
        if len(rows) >= 10000:
            raise ValueError("Capture exceeds 10000 records")
        if not 1 <= len(frame) <= original <= 1048576:
            raise ValueError("Capture record has invalid captured/original lengths")
        ether_type, ip, tags = ethernet_payload(frame)
        source, destination, protocol, transport, transport_size, ip_version = ip_payload(frame, original, ether_type, ip)
        if source not in addresses or destination not in addresses:
            raise ValueError("PCAP endpoint is not explicitly mapped to a model node")
        if protocol == 17:
            if transport_size < 8 or len(frame) < transport + 8:
                raise ValueError("PCAP UDP header is truncated")
            udp_size = struct.unpack_from("!H", frame, transport + 4)[0]
            if udp_size != transport_size or udp_size < 8:
                raise ValueError("PCAP UDP length contradicts IP payload")
            kind = "udp"
        elif protocol == 6:
            if transport_size < 20 or len(frame) < transport + 20:
                raise ValueError("PCAP TCP header is truncated")
            tcp_size = (frame[transport + 12] >> 4) * 4
            if tcp_size < 20 or tcp_size > transport_size or len(frame) < transport + tcp_size:
                raise ValueError("PCAP TCP data offset exceeds header/payload bounds")
            kind = "tcp"
        else:
            raise ValueError("PCAP IP protocol is unsupported; only TCP/UDP metadata is imported")
        source_port, destination_port = struct.unpack_from("!HH", frame, transport)
        source_label = f"[{source}]" if ip_version == "ipv6" else source
        destination_label = f"[{destination}]" if ip_version == "ipv6" else destination
        rows.append({"time_s": timestamp,
            "flow_id": f"{source_label}:{source_port}>{destination_label}:{destination_port}/{kind}" + (";vlan=" + ",".join(tags) if tags else ""),
            "source": addresses[source], "destination": addresses[destination], "wire_bytes": original})
        protocols[kind] += 1
        ip_versions[ip_version] += 1
        truncated += len(frame) < original
        vlan_records += bool(tags)
        vlan_depth = max(vlan_depth, len(tags))
    if not rows:
        raise ValueError("PCAP contains no packet records")
    canonical_map = json.dumps(address_map, sort_keys=True, separators=(",", ":")).encode()
    return rows, {"link_type": "ethernet_ipv4" if not ip_versions["ipv6"] else "ethernet_ip",
        "record_count": len(rows), "protocol_counts": protocols, "truncated_records": truncated,
        "vlan_tagged_records": vlan_records, "max_vlan_depth": vlan_depth,
        "ip_version_counts": ip_versions,
        "address_map_sha256": hashlib.sha256(canonical_map).hexdigest(),
        "parser_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "wire_bytes_scope": "PCAP original record length including recorded link header; no added preamble/interframe gap/FCS",
        "limitations": "Structural header checks only; checksums and payload are not validated/exported; no TCP/UDP protocol state, fragmentation replay or VLAN forwarding/priority semantics"}


def parse(data, address_map, node_count):
    if len(data) > 4 * 1024 * 1024:
        raise ValueError("PCAP input exceeds 4 MiB")
    if len(data) < 24:
        raise ValueError("Truncated PCAP file header")
    if data[:4] not in MAGICS:
        raise ValueError("Unsupported classic PCAP magic; select the PCAPNG format for PCAPNG input")
    endian, resolution = MAGICS[data[:4]]
    major, minor, _, _, snaplen, linktype = struct.unpack_from(endian + "HHIIII", data, 4)
    if (major, minor) != (2, 4) or not 1 <= snaplen <= 1048576 or linktype != 1:
        raise ValueError("PCAP requires version 2.4, bounded snaplen and plain Ethernet link type 1 without additional flags")
    def records():
        offset = 24
        while offset < len(data):
            if len(data) - offset < 16:
                raise ValueError("Truncated PCAP record header")
            seconds, fraction, captured, original = struct.unpack_from(endian + "IIII", data, offset)
            offset += 16
            if fraction >= resolution or not 1 <= captured <= min(snaplen, original) or not 1 <= original <= 1048576:
                raise ValueError("PCAP record has invalid timestamp/length fields")
            if captured > len(data) - offset:
                raise ValueError("PCAP record extends beyond input")
            frame = data[offset:offset + captured]; offset += captured
            yield Decimal(seconds) + Decimal(fraction) / resolution, frame, original
    rows, metadata = decode_records(records(), address_map, node_count)
    metadata.update(version="2.4", byte_order="little" if endian == "<" else "big",
                    timestamp_resolution="microseconds" if resolution == 1000000 else "nanoseconds")
    return rows, metadata
