# KNS TCP Protocol Specification

## 1. Scope

This document specifies the TCP behavior currently modeled by KNS.

KNS does not attempt to implement every aspect of RFC-compliant TCP. It implements a deterministic simulation-oriented subset suitable for experimenting with packet transmission, sequencing, acknowledgements, and connection state transitions.

## 2. Segment Model

A TCP segment in KNS contains, at minimum:

* sequence number (`seq`);
* acknowledgement number (`ack`);
* advertised window (`window`);
* TCP flags;
* payload.

The packet layer wraps the TCP segment in a `Packet`, which additionally carries:

* source;
* destination;
* current node;
* creation time;
* departure time;
* packet size;
* hop count;
* session ID;
* inferred packet type.

## 3. TCP Flags

The protocol currently uses:

```text
SYN
ACK
FIN
PSH
```

DATA segments are represented using:

```text
ACK + PSH
```

SYN-ACK uses:

```text
SYN + ACK
```

FIN segments use:

```text
FIN + ACK
```

## 4. Packet Types

KNS derives the following logical packet types from TCP flags:

```text
SYN
SYN_ACK
ACK
DATA
FIN
```

Classification precedence is:

```text
SYN + ACK   -> SYN_ACK
FIN + ACK   -> FIN
SYN         -> SYN
FIN         -> FIN
PSH         -> DATA
ACK         -> ACK
otherwise   -> DATA
```

The classification is used by `PacketReceivedEvent` to decide which transport operation to execute.

## 5. Initial Sequence Numbers

An endpoint generates an initial sequence number when initiating a SYN.

The current implementation obtains this value from the KNS deterministic random generator.

The peer's sequence number is used to establish the initial expected acknowledgement number.

## 6. Three-Way Handshake

### 6.1 Client sends SYN

Initial state:

```text
CLOSED
```

The client performs an active open:

```text
CLOSED -> SYN_SENT
```

The generated segment contains:

```text
SYN = 1
SEQ  = client ISN
ACK  = 0
```

### 6.2 Server receives SYN

The server records:

```text
expected ACK = received SEQ + 1
```

and changes state:

```text
CLOSED -> SYN_RECEIVED
```

It sends:

```text
SYN = 1
ACK = 1
```

with:

```text
SEQ = server ISN
ACK = client ISN + 1
```

### 6.3 Client receives SYN-ACK

The client validates:

```text
received ACK == client ISN + 1
```

and records:

```text
expected ACK = server ISN + 1
```

The client then enters:

```text
SYN_SENT -> ESTABLISHED
```

and sends the final ACK:

```text
ACK = server ISN + 1
```

### 6.4 Server receives final ACK

The server validates:

```text
received ACK == server ISN + 1
```

and changes:

```text
SYN_RECEIVED -> ESTABLISHED
```

When both endpoints are `ESTABLISHED`, the `TCPSession` is considered established and application DATA generation begins.

## 7. DATA Segments

A DATA segment is generated with:

```text
flags = ACK + PSH
```

The sender uses its current sequence number:

```text
SEQ = sender.seq_num
```

and current expected acknowledgement number:

```text
ACK = sender.expected_ack_num
```

The payload size is based on the configured packet size.

After constructing the DATA segment:

```text
sender.seq_num += payload_size
```

This provides a sequence-number space suitable for later implementation of loss recovery and reordering.

## 8. DATA Reception

When DATA reaches the destination:

```text
new expected ACK =
    received SEQ + payload length
```

The receiver then generates a cumulative ACK.

The ACK segment contains:

```text
ACK = new expected ACK
```

This is currently an immediate ACK model.

Delayed ACK is planned but not yet implemented.

## 9. DATA Reliability Status

The current implementation performs sequence tracking and cumulative acknowledgement, but does not yet provide full reliable TCP delivery.

Not currently implemented:

* retransmission timers;
* RTO calculation;
* retransmission after loss;
* duplicate ACK counting;
* fast retransmit;
* sliding window;
* receive reordering;
* congestion control.

Consequently, packet loss can currently prevent reliable completion of DATA transfer.

## 10. Active Close

When the client is `ESTABLISHED`, `TCPConnectionCloseEvent` initiates the active close.

The endpoint transitions:

```text
ESTABLISHED -> FIN_WAIT_1
```

and sends:

```text
FIN + ACK
```

### ACK of FIN

When the ACK reaches the client:

```text
FIN_WAIT_1 -> FIN_WAIT_2
```

### Peer FIN

When the peer's FIN arrives:

```text
FIN_WAIT_2 -> TIME_WAIT
```

The acknowledgement number is updated to:

```text
peer FIN SEQ + 1
```

## 11. Passive Close

When an `ESTABLISHED` endpoint receives a FIN:

```text
ESTABLISHED -> CLOSE_WAIT
```

It responds with an ACK and then sends its own FIN:

```text
CLOSE_WAIT -> LAST_ACK
```

After its FIN is acknowledged:

```text
LAST_ACK -> CLOSED
```

## 12. TIME_WAIT

The current state machine supports `TIME_WAIT`.

However, the current implementation does not yet schedule expiration of `TIME_WAIT`.

Therefore the protocol currently has the state transition but not the full timer-driven lifecycle.

## 13. Packet Transmission

TCP itself does not choose the physical path.

After constructing a segment, the protocol wraps it in a `Packet` and passes it to the network layer.

The path is selected from the routing table maintained by `SimulationEngine`.

The network then models:

```text
routing
    +
bandwidth
    +
propagation delay
    +
link mode
    +
packet loss
```

before scheduling delivery.

## 14. Connection Model

The current protocol model uses one `TCPSession` containing two known `TCPConnection` endpoints:

```text
TCPSession
├── client TCPConnection
└── server TCPConnection
```

This is intentionally simpler than a real TCP socket architecture.

A future protocol model may introduce:

* listening sockets;
* local/remote ports;
* connection acceptance;
* multiple concurrent connections per node;
* RST handling.

## 15. Protocol Roadmap

The intended protocol evolution is:

```text
Three-way handshake
        ↓
Sequence-aware DATA
        ↓
Cumulative ACK
        ↓
Sliding window
        ↓
RTO / retransmission
        ↓
Duplicate ACK detection
        ↓
Fast retransmit
        ↓
Congestion control
```

Each stage should preserve compatibility with the event-driven simulation architecture.

## 16. Non-Goals of the Current Specification

The current protocol does not attempt to model:

* IP;
* Ethernet;
* ARP;
* real socket APIs;
* application protocols;
* real-world TCP option negotiation;
* checksum calculation;
* packet fragmentation/reassembly;
* full RFC conformance.

Those may become separate layers or future extensions of KNS.
