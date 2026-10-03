# TCP serial-number space

Issue #138 is valid. Sequence order is modular uint32 order, not integer order.
All accepted flight and receive ranges must be strictly shorter than 2^31 bytes.
The exact half-space distance is ambiguous and rejected. ACK validity is a
forward distance from SND.UNA bounded by current flight. Buffers use the same
comparison rules. Receive insertion rejects stale, overlapping and out-of-range
segments without changing buffered bytes; ordering is relative to RCV.NXT.
Capacity zero retains the standalone unbounded-capacity convention but does not
remove the serial-space bound. Configured windows >= 2^31 throw.

Plan: central constexpr serial helpers, replace ACK and buffer comparisons,
validate window ranges, and test in-order/out-of-order wrap, duplicates,
overlaps, stale/future ACKs and bytes in flight. Run full CTest and headless.
