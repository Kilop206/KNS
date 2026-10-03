# Independent run random streams

Issue #148 remains valid after the runner fixes. RunConfig currently seeds a
process-global generator used by losses and active TCP ISNs.

Each engine must own its RNG, seeded before work, and supply every random draw
for that run. TCP SYN retries retain their original ISN. Standalone connection
and link helpers own their fallback streams rather than process-global state.
No RNG pointer may outlive an engine or be invalidated by moving it.

Plan: make Random a value type, add it to owners, pass an explicit ISN for engine
handshakes and use the engine stream for loss decisions. Compare identical runs
alone and interleaved with differently seeded engines, including ISNs and losses.
Build, run full CTest and headless CSV integration before completion.

Implemented and validated: 277 CTest cases passed, including independent draws
and end-to-end interleaved traffic with equal ISNs, loss counts and logical time.
