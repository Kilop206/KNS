# Resumable TCP Generation - Issue #121

Still valid at investigation on `tcp` at `4562f03`: generation stopped on a full
send window and ACK handling could not restart an already started workload.

## Required Contract

`generatePackets` starts a workload once and subsequently resumes it without
resetting its packet count. Each session has at most one internally scheduled
generation event. Executing that event clears the pending flag before attempting
one send. A full window suspends generation without polling; incoming ACKs invoke
the same scheduling gate. Completion and non-ESTABLISHED sessions do not schedule
generation. Calling the API repeatedly while an event is pending is idempotent.

A configured payload exceeding the local send window raises `std::invalid_argument`
before scheduling or starting a workload. After an explicit local window change,
the caller can invoke `generatePackets` to resume. Peer window tracking is #144.

## Plan and Tests

Add session pending-event state, centralize generator scheduling in the engine,
and route continuations and ACK-triggered resumption through that gate. Test a
workload larger than the window, repeated scheduling requests, no events while
window-blocked, and invalid oversized payload without partial state. Verify exact
received byte progress and normal close. Build and run focused/full CTest and
headless validation. Commit specification, code/tests, and evidence separately.

## Completion Record

Specification `54b23cd`; implementation `844bff0`. MinGW/Ninja build succeeded;
all 271 CTest cases passed. Two new tests cover complete delivery through a
one-packet window, pending-event deduplication, and oversized payload rejection.
The listener-port regression now asserts completed handshakes and response ports
without using the started-workload flag to suppress generation.

The mesh4 headless run from `build/validation-bin` delivered 148 packets (80 DATA)
with zero loss and zero packets in transit. The changes are committed locally;
the GitHub issue remains open pending publication.
