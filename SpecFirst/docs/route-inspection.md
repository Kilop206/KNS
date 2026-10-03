# Route inspection

## Requirement

Expose the simulator's current forwarding path for an ordered pair of devices
without scheduling traffic, advancing time, consuming randomness, or changing
topology. The desktop canvas provides a Route tool alongside Cable and TCP.

## Contract

`SimulationEngine::traceRoute` follows the actual routing entry at each hop,
including its stable link ID. It refreshes routing through the existing lookup
API and respects the selected metric, link availability, and simplex direction.
It does not reconstruct a separate source-only Dijkstra path: forwarding may
make a different choice at an intermediate node.

The result distinguishes reachable, unreachable, invalid/inactive endpoints,
and a forwarding loop. A loop returns the finite traversed prefix, including
the hop which revisits a node. Other failures may retain a traversed prefix.
An active source equal to destination is reachable with zero hops.

Each hop reports its endpoints, link identity, propagation delay, and capacity.
Total delay is the sum over the reported hops. Bottleneck capacity is absent
for an empty path. These are configured path characteristics, not measured RTT,
queueing delay, or throughput.

## Canvas and acceptance criteria

- Route selects source then destination and highlights the exact chosen links.
- Show endpoint IDs, status, metric, hop count, propagation delay and capacity.
- Edits and routing metric changes refresh the preview; deleted endpoints clear it.
- Escape, reset, or switching tools clears the preview.
- Inspection never requests a TCP connection or creates a cable/session/event.
- Tests cover all metrics, parallel links, direction, DOWN links, node removal,
  self routes, unreachable endpoints, and live canvas updates. Loop detection is
  a defensive bound if future forwarding policies allow revisiting a node.

## Validation

Built with the Windows MinGW/Ninja toolchain. All 318 C++/headless CTest cases
passed; `runner_tests` passed separately outside the restricted sandbox (319
registered tests total). The focused tests cover API results and actual canvas
link geometry before and after a routing metric change, link failure/recovery,
endpoint deletion, Escape, tool switching and reset. OpenGL captures of the
direct and bandwidth-selected paths were generated and visually inspected.
