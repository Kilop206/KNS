"""Strict bounded single-section/interface PCAPNG metadata adapter.

Supports SHB/IDB/EPB only, both endiannesses, decimal/binary timestamp scales.
See trace-import.md for the exact subset, primary reference and byte scope.
"""
from decimal import Decimal, localcontext
import hashlib
from pathlib import Path
import struct

import pcap_trace

SHB = 0x0A0D0D0A


def block(data, offset, endian):
    if len(data) - offset < 12:
        raise ValueError("Truncated PCAPNG block header/trailer")
    kind, size = struct.unpack_from(endian + "II", data, offset)
    if size < 12 or size % 4 or size > len(data) - offset:
        raise ValueError("PCAPNG block length violates alignment/input bounds")
    if struct.unpack_from(endian + "I", data, offset + size - 4)[0] != size:
        raise ValueError("PCAPNG repeated block lengths disagree")
    return kind, data[offset + 8:offset + size - 4], offset + size


def options(data, endian):
    result, offset = [], 0
    while offset < len(data):
        if len(data) - offset < 4:
            raise ValueError("Truncated PCAPNG option header")
        code, size = struct.unpack_from(endian + "HH", data, offset)
        offset += 4
        end = offset + ((size + 3) & ~3)
        if end > len(data):
            raise ValueError("PCAPNG option length exceeds block")
        if code == 0:
            if size or offset != len(data):
                raise ValueError("Invalid PCAPNG end-of-options")
            return result
        if len(result) >= 4096:
            raise ValueError("PCAPNG block exceeds 4096 options")
        result.append((code, data[offset:offset + size]))
        offset = end
    return result


def parse(data, address_map, node_count):
    if len(data) > 4 * 1024 * 1024:
        raise ValueError("PCAPNG input exceeds 4 MiB")
    if len(data) < 28 or data[:4] != b"\x0a\x0d\x0d\x0a":
        raise ValueError("PCAPNG requires a complete initial Section Header Block")
    orders = {b"\x4d\x3c\x2b\x1a": "<", b"\x1a\x2b\x3c\x4d": ">"}
    if data[8:12] not in orders:
        raise ValueError("Invalid PCAPNG byte-order magic")
    endian = orders[data[8:12]]
    _, body, offset = block(data, 0, endian)
    if len(body) < 16:
        raise ValueError("Truncated PCAPNG section body")
    _, major, minor, length = struct.unpack_from(endian + "IHHq", body)
    if (major, minor) != (1, 0) or length not in (-1, len(data) - offset):
        raise ValueError("PCAPNG requires version 1.0 and a consistent single-section length")
    discarded = len(options(body[16:], endian))
    interface = None
    records, counts = [], {"section": 1, "interface": 0, "enhanced_packet": 0}
    while offset < len(data):
        kind, body, offset = block(data, offset, endian)
        if kind == 1:
            if interface is not None or len(body) < 8:
                raise ValueError("PCAPNG requires exactly one complete Interface Description Block")
            linktype, _, snaplen = struct.unpack_from(endian + "HHI", body)
            if linktype != 1 or snaplen > 1048576:
                raise ValueError("PCAPNG requires Ethernet link type 1 and bounded snaplen (zero means unlimited)")
            base, exponent, time_offset, seen = 10, 6, 0, set()
            for code, value in options(body[8:], endian):
                if code in (9, 14):
                    if code in seen or len(value) != (1 if code == 9 else 8):
                        raise ValueError("Duplicate or invalid PCAPNG timestamp option")
                    seen.add(code)
                    if code == 9:
                        base, exponent = (2 if value[0] & 128 else 10), value[0] & 127
                    else:
                        time_offset = struct.unpack(endian + "q", value)[0]
                else:
                    discarded += 1
            interface = (snaplen or 1048576, base, exponent, time_offset)
            counts["interface"] += 1
        elif kind == 6:
            if interface is None or len(body) < 20:
                raise ValueError("PCAPNG enhanced packets require an existing interface and complete header")
            identifier, high, low, captured, original = struct.unpack_from(endian + "IIIII", body)
            snaplen, base, exponent, time_offset = interface
            end = 20 + ((captured + 3) & ~3)
            if identifier != 0 or not 1 <= captured <= min(snaplen, original) or not 1 <= original <= 1048576 or end > len(body):
                raise ValueError("PCAPNG enhanced packet has invalid interface/length bounds")
            if len(records) >= 10000:
                raise ValueError("PCAPNG exceeds 10000 packet records")
            discarded += len(options(body[end:], endian))
            with localcontext() as context:
                context.prec = 200
                timestamp = Decimal((high << 32) | low) / (Decimal(base) ** exponent) + time_offset
            records.append((timestamp, body[20:20 + captured], original))
            counts["enhanced_packet"] += 1
        else:
            raise ValueError("Unsupported PCAPNG block; only one SHB, one Ethernet IDB and timestamped EPBs are accepted")
    rows, metadata = pcap_trace.decode_records(records, address_map, node_count)
    metadata["decoder_sha256"] = metadata["parser_sha256"]
    metadata.update(version="1.0", byte_order="little" if endian == "<" else "big",
        block_counts=counts, timestamp_resolution={"base": interface[1], "exponent": interface[2]},
        timestamp_offset_applied=bool(interface[3]), discarded_option_count=discarded,
        parser_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        wire_bytes_scope="PCAPNG original packet length including recorded link header, excluding block/option padding; no added preamble/interframe gap/FCS")
    return rows, metadata
