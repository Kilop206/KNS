# Fresh topology state for each engine

Issue #139 is valid: constructing an engine shallow-copies links and inherits
queues and serialization timestamps. Issue #141 is valid: GUI setup unconditionally
overwrites file-defined loss probabilities.

## Contract

Engine construction takes an independent configuration snapshot. Preserve node,
interface and link identities, labels, active/up state and all link parameters;
reset queues, busy times and RNG state. Source topology and other engines are
unchanged by running or mutating the new engine. Routing revisions are shared
only inside a snapshot. Ordinary Topology copies retain their existing semantics;
an explicit cloneForRun boundary provides isolation.

GUI loss override is disabled by default and explicitly enabled/disabled by a
checkbox. Disabling restores source per-link loss by identity; changing the
slider applies only while enabled. Restart preserves the explicit override mode.

## Acceptance and plan

Add snapshot cloning and restart/isolation tests, adapting tests which previously
mutated source links to instead obtain the engine-owned link. Test nonempty
queues, completed transmissions, parallel IDs and preserved configuration.
Then implement GUI override and test shared configuration behavior. Build, full
CTest, headless integration and review GUI call sites before completion.
