# TCP payload goodput

Runtime Analytics exposes `tcp_contiguous_payload_bytes` and
`tcp_payload_goodput_bps`. The byte counter advances only when a DATA receive
operation advances the modeled receiver's contiguous sequence frontier. It
excludes headers, handshake/ACK/FIN traffic, rejected payload and duplicate
segments. Out-of-order bytes remain uncounted until their gap closes; one
receive operation may then release more bytes than that packet contains.

The sequence delta uses unsigned 32-bit arithmetic, matching the bounded TCP
receive window across sequence wrap. Flow groups separate session, direction
and endpoint ports. Destination groups aggregate released bytes without
counting forwarded hops. Group overflow has separate explicit byte counters.

The root rate is `8 * released_bytes / collection_until_s`, in bits per virtual
second. A sampled rate uses newly released bytes divided by that sample's virtual
duration. Zero-duration rates are null. Root counters may include deliveries tied
to the collection horizon after the last sample callback; this existing event-ID
boundary rule also applies to goodput. Retention never resets cumulative bytes.

This measures transport payload release, not application reads, successful HTTP
transactions or durable delivery. Service datagrams and abstract network packets
do not contribute. Never compare it as if it measured their application goodput.
Changing receiver behavior can change this metric without changing packet arrival
counts. No new acknowledgement behavior, RNG draws or packet scheduling is added.

The existing optional `--analytics` CLI path includes these additive fields.
The offline exporter preserves blank values for older reports lacking them.
Validation checks cumulative monotonicity, group/overflow conservation and rate
denominators; small counts remain descriptive observations.
