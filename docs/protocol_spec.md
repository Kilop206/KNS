# KNS TCP Protocol Specification

## Scope

This specification records the TCP subset implemented by KNS. It describes
observable simulator behavior, not full RFC compatibility. Architectural
ownership and implementation boundaries are described in
[`tcp_design.md`](tcp_design.md).

## Segment and packet model

`TCPSegment` contains:

- 16-bit source and destination ports;
- 32-bit sequence and acknowledgement numbers;
- a 16-bit advertised window;
- SYN, ACK, FIN, RST, and PSH flags;
- a byte payload.

`Packet` adds the source, destination, current and previous node, simulation
timestamps, packet size, hop count, session ID, last-link ID, and inferred
`PacketType` used by the event dispatcher.

Packet classification has this precedence:

```text
RST         → RST
SYN + ACK   → SYN_ACK
FIN + ACK   → FIN
SYN         → SYN
FIN         → FIN
PSH         → DATA
ACK         → ACK
otherwise   → DATA
```

## Endpoint identity and dispatch

A connection endpoint is identified by node and TCP port. A packet correlated
to an existing session is dispatched only if its source/destination nodes and
ports match one direction of that session. This prevents an unrelated packet
from taking over a colliding numeric session ID.

Passive listeners are keyed by `(node, port)`. A SYN with no matching session is
accepted only when that listener exists, is active, and has backlog capacity.
Successful acceptance creates a new zero-based session ID and server endpoint.

## Connection establishment

### Active open

The client starts in `CLOSED`. `send_syn()` selects a deterministic random ISN,
sets `SND.UNA`, and transitions to `SYN_SENT`.

```text
flags = SYN
seq   = client ISN
ack   = 0
```

The SYN timeout is one simulated second. While the client remains in
`SYN_SENT`, the same SYN may be retransmitted up to
`TCPConnection::MAX_SYN_RETRIES` (5 retries).

### Passive open

The accepted server endpoint follows:

```text
LISTEN → SYN_RECEIVED
RCV.NXT = client ISN + 1
```

It replies with:

```text
flags = SYN | ACK
seq   = server local sequence (0 for a newly accepted session)
ack   = client ISN + 1
```

### Final ACK

The client accepts the SYN-ACK only in `SYN_SENT` and only when its ACK equals
`client ISN + 1`. It records the server sequence plus one, enters `ESTABLISHED`,
and sends a plain ACK. The server validates its local sequence plus one in
`SYN_RECEIVED` and enters `ESTABLISHED`. DATA generation begins only after both
endpoints are established.

## Unavailable passive opens and RST

If the destination has no listener on the requested port, the listener is not
active, or its backlog is full, the receiver sends:

```text
flags = RST | ACK
seq   = 0
ack   = received SYN seq + 1
```

The reset reverses the original node and port direction and preserves the
incoming correlation ID. Rejection creates no session. An unmatched incoming
RST is dropped without a reply, preventing RST loops. A matched RST closes the
receiving endpoint through the retransmission-failure path.

## DATA transmission

Application DATA is represented as:

```text
flags = ACK | PSH
seq   = SND.NXT
ack   = current RCV.NXT
```

The payload length consumes the same number of sequence values. A successfully
transmitted segment is stored in `TCPSendBuffer`, after which `SND.NXT` advances.
The local send-window check prevents the outstanding byte range plus the new
payload from exceeding the configured limit.

The first outstanding segment starts an RTO event. After cumulative ACK
advancement removes it, the new oldest segment receives the next effective
timeout. Timeout events for data that is no longer outstanding are ignored.

## Receive ordering, cumulative ACK, and window

The receive buffer accepts unique non-empty segments at or after `RCV.NXT`,
subject to its byte capacity. Entries are kept in sequence order. Contiguous
entries starting at `RCV.NXT` are consumed and advance the cumulative
acknowledgement; later entries remain buffered until the gap arrives.

ACK-bearing control segments advertise the receiver's available buffer space,
clamped to the 16-bit TCP window field. The default receive capacity is 65,535
bytes. The normal ACK path does not yet propagate the received window field into
the sender's local limit; see the limitations in `tcp_design.md`.

## Delayed ACK

For the first in-order DATA segment, the receiver schedules an ACK for 0.2
simulated seconds later. A second in-order segment while that ACK is pending
sends an immediate cumulative ACK and clears the pending marker. Out-of-order
DATA also produces an immediate ACK for the current `RCV.NXT`.

The delayed event is ignored when the session no longer exists, the endpoint is
not established, no ACK is pending, or a newer acknowledgement made the event
obsolete.

## RTT estimation and RTO

For an original transmission acknowledged at time `t_ack`:

```text
sample = t_ack - t_sent
```

The first valid sample initializes:

```text
SRTT   = sample
RTTVAR = sample / 2
RTO    = SRTT + 4 × RTTVAR
```

Later samples use alpha 1/8 and beta 1/4. RTO is clamped to 0.2–60 simulated
seconds and starts at 1.0 second before a sample exists. A timeout doubles the
backoff, up to 64x. A valid acknowledgement sample resets that backoff.

Karn's rule applies: retransmitted entries cannot supply an RTT sample, including
when a cumulative ACK also removes them.

## Retransmission and failure

An RTO applies only to the oldest outstanding DATA segment. On expiry, the
sender notifies congestion control of loss and schedules retransmission. A
successful retransmission is marked in the send buffer and arms a new timeout
using the backed-off RTO.

Each segment permits at most five DATA retransmissions. Reaching the limit moves
the endpoint to `CLOSED` and clears send/receive buffers, RTO state, duplicate
ACK state, and any pending delayed ACK.

## Duplicate ACK and fast retransmit

An ACK equal to `SND.UNA` is a duplicate. An ACK below `SND.UNA` is stale and is
ignored; an ACK above `SND.NXT` is invalid. ACK advancement resets the duplicate
streak and cumulatively releases acknowledged send-buffer entries.

After three duplicate ACKs, `TCPFastRetransmitEvent` retransmits the oldest
outstanding segment immediately and schedules its timeout. The indication is
consumed so one duplicate streak cannot schedule repeated fast-retransmit
events.

## Congestion-control state

Every `TCPConnection` owns one of these controllers:

- Tahoe;
- Reno (default);
- NewReno;
- CUBIC.

The controller receives byte-counted ACKs, timeout loss, duplicate ACKs, fast
retransmit, and recovery ACKs as supported by the selected algorithm. It tracks
`cwnd`, `ssthresh`, MSS, and fast-recovery state and records changed congestion
samples for visualization. The current packet generator does not yet use
controller `canSend()` as an additional transmission gate.

## Connection termination

The generated workload initiates close from the client:

```text
client: ESTABLISHED → FIN_WAIT_1 → FIN_WAIT_2 → TIME_WAIT → CLOSED
server: ESTABLISHED → CLOSE_WAIT → LAST_ACK → CLOSED
```

FIN segments use `FIN|ACK` and consume one sequence number. A received FIN sets
the acknowledgement edge to `peer FIN seq + 1`. The server ACKs the client FIN,
sends its own FIN, and closes after its FIN is acknowledged.

The active closer schedules `TCPTimeWaitTimeoutEvent` on entry to `TIME_WAIT`.
The implemented hold time is 0.1 simulated seconds. Expiration transitions that
endpoint to `CLOSED`; a fully closed accepted session releases its listener's
backlog slot.

## Supported and unsupported behavior

Implemented behavior includes handshake retry, passive acceptance, per-port
backlog, unavailable-open RST, ordered send/receive buffers, cumulative and
delayed ACK, receive-window advertisement, RTT/RTO estimation, Karn's rule,
timeout and fast retransmission, retransmission failure, four congestion-control
algorithms, normal active/passive close, and `TIME_WAIT` expiration.

KNS does not currently specify full RFC behavior for TCP options, SACK,
timestamps, zero-window probing, keepalive, urgent data, checksum, arbitrary
bidirectional application streams, simultaneous-open, every simultaneous-close
transition, or every possible RST condition.
