# Simulation Time and DATA Latency

## Investigation

Issues #132 and #127 are still valid on `tcp` at `30471bf`.
Event timestamps and clock updates lack validation. CSV average latency divides
DATA-only samples by all delivered packets.

## Required Contracts

* Event timestamps are immutable, finite and non-negative. Invalid construction
  raises `std::invalid_argument`.
* Engine scheduling rejects null events and timestamps before the current clock
  without altering the queue. Same-time scheduling remains valid and preserves
  the existing timestamp/ID ordering. A standalone queue has no simulation clock.
* Clock updates reject non-finite times, negative increments, backward moves and
  overflow. Engine manual advancement cannot skip a queued event. Rejections
  leave the clock and queue unchanged.
* Packet scheduling validates requested and computed times before changing link
  reservations, statistics or queues.
* Each destination DATA packet contributing to `total_latency` also increments
  `data_packets_delivered`. Control packets contribute to neither. These are
  network delivery samples, including duplicate DATA, not unique application bytes.
* CSV `avg_latency` is total DATA latency divided by `data_packets_delivered`, or
  zero with no DATA samples. Existing column names/order remain and the explicit
  population count is appended as `data_packets_delivered`.

## Plan and Acceptance Tests

1. Commit time guards and tests covering NaN, both infinities, negative time,
   backward scheduling, equal-time execution, clock overflow and rejection state.
2. Commit the DATA sample counter and CSV integration tests comparing delivery
   observer samples with exported average, with and without control traffic.
3. Build, run focused regressions, full CTest and headless validation; record
   results and commits. GUI latency charts already consume DATA observer samples.
