# Parallel cables in the canvas

Independent cables between the same devices can model link redundancy. KNS
already routes and accounts for each link by stable identity; the canvas now
also draws each cable on its own curved lane. This applies to imported and
interactively created topologies, including reversed endpoint declarations and
simplex cables. A single cable keeps its straight path.

## Create and inspect redundancy

1. Open `app/topologies/parallel-cables-lab.json`. The Client--Edge router pair
   has a 100 Mbps primary cable and a 20 Mbps backup. Edge router--Application
   server has a single cable.
2. Right-click either Client--Edge router curve to inspect or edit that exact
   cable. The menu includes its runtime link ID. You can independently change
   its state, parameters and queue policy or delete it.
3. Choose **Route**, click Client then Application server, and open
   **Resilience**. With the delay metric, losing the primary cable produces a
   6 ms / 20 Mbps alternative instead of the baseline 2 ms / 100 Mbps. Losing
   the only router--server cable disconnects this directional pair.
4. Open **Cable settings**, enable **Allow parallel cables**, then use **Cable**
   to connect Edge router and Application server again. The next resilience
   analysis can now find an alternative for either baseline cable outage.

Parallel creation is opt-in to prevent accidental duplicates. **Reset defaults**
turns it off again. The option is an editor preference; every created cable's
configuration is saved in the normal topology JSON. Link IDs are runtime
identities and may change when reloading a file. There is no automatic link
aggregation or bandwidth pooling: the configured routing metric still chooses
one next-hop cable.

## Traffic and visual behavior

Packets follow the same cable geometry used for drawing and selection, in the
correct direction. Only the cable identified by the packet is marked busy.
Packet tooltips include the link ID and are restricted to the visible canvas.
Arrived packets are not rendered. Older visual packets without a link identity
fall back to the straight endpoint path and do not mark a guessed cable busy.

Dragging devices, panning and zooming update the geometry. Lanes are ordered by
link identity; adding or removing parallel cables recenters the remaining bundle.
If a cable disappears while an animation is active, its last known lane remains
available until the packet arrives, using the retained endpoint coordinates.
This preserves the existing complete-accepted-hop simulation policy. A packet
whose cable was never observed by the canvas uses the straight fallback.

At very low zoom, lane spacing retains a minimum screen separation for selection.
Overlapping device icons can still obscure the ends of cables; separate the
devices or zoom in to inspect them. The canvas behavior does not change queue,
routing, transmission or loss semantics in the simulation engine.
