# Canvas cable settings

## Requirement

Allow desktop users to construct networks with configurable cable properties.
The existing Link model already supports these properties; the canvas must use
its validation and queue safety rules without introducing simulation side effects.

## Contract and acceptance criteria

- Cable settings apply to subsequently created canvas cables only. Defaults stay
  at 100 Mbps, 1 ms, zero loss, full duplex, and capacity 32.
- Users can set bandwidth, propagation delay, loss percentage, queue capacity,
  and full-duplex, half-duplex, or simplex mode. The first clicked device is the
  simplex transmitter and the second is its receiver.
- Invalid values report the core validation error; creation leaves the topology
  unchanged. Existing duplicate-cable prevention remains in place.
- Existing cable menus expose mode and queue capacity. Core setters reject mode
  changes during pending transmissions and shrinking a queue below occupancy.
  Failed edits preserve the previous setting and show the error in the menu.
- Mode changes preserve cable identity and refresh routing through the existing
  revision mechanism. Configuration persists through topology JSON round trips.
- Settings, creation, and edits neither start TCP nor process simulation events.
- Tests exercise the real canvas controls, all three transmission modes, custom
  properties, invalid creation, safe edits, and persistence. Check rendered menus.
