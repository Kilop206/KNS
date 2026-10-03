# TCP serialized size

Issue #143 is valid: TCP control packets currently inherit application payload
size. The model will use fixed IPv4 (20 byte) and TCP (20 byte) headers, without
options or link-layer framing. DATA wire size is 40 + actual payload bytes;
SYN, SYN-ACK, ACK, FIN and RST without payload occupy 40 bytes.

Global packet size remains application payload size. The transmission boundary
normalizes TCP packets before timing, queuing, observers and arrival events,
including retransmissions and forwarded packets (no cumulative header growth).
Raw packets with no TCP flags/payload retain explicit caller-provided wire size.
Overflow is rejected before state mutation.

Acceptance: all control types have identical serialization under different
global payload sizes; DATA includes its payload once. Build, full CTest and
headless output must remain valid.
