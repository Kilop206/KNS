# KNS TCP Design

## Purpose and scope

KNS implements a deterministic, event-driven TCP model for network simulation.
It is intentionally smaller than a host operating system's TCP stack, but the
model includes connection establishment and termination, passive listeners,
buffered data transfer, loss recovery, delayed acknowledgements, receive-window
advertisement, and congestion-control state.

This document describes the behavior present in the current branch. Wire-level
rules and numeric timer values are summarized in
[`protocol_spec.md`](protocol_spec.md).

## Architecture

```text
SimulationEngine
├── SimulationClock and EventQueue
├── Topology, Routing, and Link objects
├── TCPListener registry keyed by (node, port)
└── TCPSession registry keyed by session ID
    ├── client TCPConnection
    └── server TCPConnection
        ├── TCPStateMachine
        ├── TCPSendBuffer and TCPReceiveBuffer
        ├── RTOManager and RTTEstimator
        ├── TCPLossDetector
        └── CongestionControl
```

`SimulationEngine` owns the simulation clock, event queue, topology, routing
tables, listeners, sessions, packet statistics, and in-flight packet records.
TCP never manipulates a physical path directly: events construct a `Packet`,
then `PacketUtils::sendPacketThroughTopology()` asks the engine for the next hop
and schedules a `PacketReceivedEvent`.

### Session and endpoint ownership

A `TCPSession` owns two `TCPConnection` objects. Each connection stores its own
state machine, node and port tuple, sequence state, buffers, timers, loss
detector, and congestion controller. `TCPSession::getState()` derives an
aggregate state from both endpoints; protocol decisions remain endpoint-local.

Session IDs are zero-based and independent from the TCP four-tuple. Incoming
packets must match both the session ID and endpoint tuple before dispatch.

### Passive listeners

`SimulationEngine::startTCPListen()` registers a `TCPListener` by node and port.
On an unmatched SYN, `PacketReceivedEvent` asks the matching listener to accept
the connection. Acceptance creates a new session and continues the normal
server-side handshake.

A positive backlog limits concurrently tracked accepted sessions. Closed or
cancelled sessions release their listener slot. A missing listener, unopened
port, stopped listener, or full backlog rejects the SYN with `RST|ACK` without
creating a session. Received RST packets with no matching session are ignored,
which prevents reset loops.

## State model

The endpoint state machine defines:

```text
CLOSED, LISTEN, SYN_SENT, SYN_RECEIVED, ESTABLISHED,
FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT
```

The normal active-open path is:

```text
CLOSED → SYN_SENT → ESTABLISHED
```

The passive-open path is:

```text
LISTEN → SYN_RECEIVED → ESTABLISHED
```

The generated workload closes from the client side:

```text
client: ESTABLISHED → FIN_WAIT_1 → FIN_WAIT_2 → TIME_WAIT → CLOSED
server: ESTABLISHED → CLOSE_WAIT → LAST_ACK → CLOSED
```

Receiving a peer FIN while in `FIN_WAIT_1` enters `CLOSING`. Invalid transitions
are rejected by `TCPStateMachine` rather than silently changing state.

## Event-driven lifecycle

The main TCP events are:

| Event | Responsibility |
| --- | --- |
| `TCPHandshakeEvent` | Sends the initial SYN. |
| `TCPHandshakeTimeoutEvent` | Retries an unanswered SYN. |
| `PacketGenerationEvent` | Queues and sends DATA after establishment. |
| `PacketReceivedEvent` | Dispatches flags, ACKs, DATA, FIN, and RST. |
| `TCPDelayedAckEvent` | Sends an ACK if the pending acknowledgement is still current. |
| `TCPTimeoutEvent` | Detects expiry of the oldest outstanding DATA segment. |
| `TCPRetransmissionEvent` | Retransmits timed-out DATA and rearms its timeout. |
| `TCPFastRetransmitEvent` | Retransmits the oldest outstanding segment after duplicate ACKs. |
| `TCPConnectionCloseEvent` | Starts the generated workload's active close. |
| `TCPTimeWaitTimeoutEvent` | Moves the active closer from `TIME_WAIT` to `CLOSED`. |

Events carry session IDs rather than owning session references. Events for a
cancelled session safely become no-ops.

## Data transfer and buffering

DATA uses `ACK|PSH`. `PacketGenerationEvent` takes `SND.NXT` from the client,
queues the segment in `TCPSendBuffer`, advances the sequence space by payload
length, and starts a timeout when the buffer changes from empty to non-empty.
Cumulative ACKs remove all fully acknowledged entries and advance `SND.UNA`.

`TCPReceiveBuffer` stores unique segments ordered by sequence number. It rejects
old, duplicate, empty, or over-capacity entries and consumes contiguous entries
from the current receive edge. Out-of-order segments remain buffered until a gap
is filled. ACKs therefore advertise the highest contiguous byte received, not
the greatest sequence number observed.

The default local send and receive windows are 65,535 bytes. ACK, SYN, SYN-ACK,
and FIN segments advertise the receiver's currently available buffer space.

## Acknowledgement policy

The first in-order DATA segment starts a delayed ACK. The delay is 0.2 simulated
seconds. A second in-order segment while an ACK is pending causes an immediate
cumulative ACK. Out-of-order DATA is ACKed immediately so the missing sequence
edge is visible to the sender.

A delayed event sends only if the connection is still established, an ACK is
still pending, and the expected acknowledgement number has not changed. This
makes superseded delayed-ACK events harmless.

## Loss recovery and timers

`RTTEstimator` maintains SRTT and RTTVAR using alpha 1/8, beta 1/4, and K=4.
The initial RTO is 1.0 second and the result is clamped to 0.2–60 seconds.
`RTOManager` applies exponential backoff up to 64x after timeouts and resets the
backoff when a valid RTT sample is acknowledged.

Karn's rule is implemented: ACKs for retransmitted data do not produce RTT
samples. Only the oldest outstanding segment owns the effective timeout; stale
timeout events return without changing state. DATA permits at most five
retransmissions per segment. Exhaustion closes the endpoint and clears its
buffers, timer state, delayed ACK, and loss detector.

`TCPLossDetector` tracks ACKs equal to `SND.UNA`. Three duplicate ACKs schedule a
fast retransmission of the oldest outstanding segment. Advancing or stale ACKs
do not extend the duplicate streak.

## Congestion control

The congestion layer implements Tahoe, Reno, NewReno, and CUBIC behind the
`CongestionControl` interface. Controllers maintain `cwnd`, `ssthresh`, and MSS;
receive ACK, timeout-loss, duplicate-ACK, fast-retransmit, and recovery signals;
and expose timestamped congestion-history samples for the GUI.

Reno is the default controller created by `TCPConnection`. The algorithms and
their transitions are unit-tested independently and through ACK/loss paths.

## Connection termination

After all generated DATA is acknowledged, the engine schedules an active close.
FIN consumes one sequence number. The peer ACKs the FIN, enters `CLOSE_WAIT`,
then sends its own FIN and waits in `LAST_ACK`. The active closer ACKs that FIN,
enters `TIME_WAIT`, and schedules `TCPTimeWaitTimeoutEvent` for 0.1 simulated
seconds later. When both endpoints are closed, any listener backlog slot is
released.

## Current limitations

Implemented mechanisms should not be confused with full RFC conformance. The
current model still has these deliberate limitations:

- no IP, Ethernet, ARP, checksum, fragmentation, or application protocol layer;
- no TCP option negotiation, SACK, timestamps, persist timer, keepalive, Nagle,
  or silly-window avoidance;
- generated application traffic is client-to-server and uses a fixed engine
  workload rather than a socket API or arbitrary byte stream;
- the advertised peer window is represented on segments, but normal ACK
  processing does not yet copy it into the sender's local send-window limit;
- congestion controllers maintain and expose their algorithms, but
  `PacketGenerationEvent` currently gates new data with the send window rather
  than the selected controller's `canSend()` result;
- handshake retry uses a fixed one-second timeout and is separate from the DATA
  RTT/RTO estimator;
- simultaneous-open and all simultaneous-close edge cases are not modeled as a
  complete RFC state machine;
- reset generation is focused on unavailable passive opens; it is not a general
  implementation of every RFC reset rule.

These are future integration or protocol-expansion areas. Delayed ACK, DATA
retransmission, buffers, receive reordering, fast retransmit, congestion-control
algorithms, passive listeners, RST rejection, and `TIME_WAIT` expiration are
already implemented.
