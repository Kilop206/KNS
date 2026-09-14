# Simulation Time and DATA Latency

## Investigation

Issues #132 and #127 were still valid on `tcp` at `30471bf`.
Event timestamps and clock updates lacked validation. CSV average latency divided
DATA-only samples by all delivered packets. Both contracts below are now implemented.

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

## Validation Record

Specification commit: `34e798f`. Time guards: `9388a61` (#132).
DATA latency: `587d335` (#127).

The full MinGW/Ninja C++20 build succeeded and all 262 CTest cases passed,
including five new regression cases. The headless mesh4 run delivered 148
packets (80 DATA), with zero loss or packets in transit. Its CSV reports
`total_latency=0.6808` and `avg_latency=0.00851`, exactly the DATA sample mean.

The original `build/app/KNS.exe` could not be overwritten (linker access denied,
also outside the sandbox). The local CMake cache now sets
`CMAKE_RUNTIME_OUTPUT_DIRECTORY` to `build/validation-bin`; the successful build,
CTest and smoke run use those new executables. No source build configuration
was changed for this workaround. Commits remain local and issues remain open.
