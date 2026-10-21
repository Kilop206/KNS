# Link queue management

Links use drop-tail by default. Capacity counts scheduled transmissions until
their arrival event releases the queue entry. Full duplex has independent queues
in each direction; half duplex shares a queue, and simplex admits only A to B.

`Link::configureRed(min, max, probability, weight)` enables early drops. Thresholds
must satisfy `0 <= min < max <= capacity`, probability must be in `[0, 1]`, and
weight must be finite and in `(0, 1]`.

The default weight is `1`, preserving the original instantaneous queue policy.
A smaller weight enables an exponentially weighted moving average (EWMA).
On each admission attempt that has queue capacity, the engine samples the
current occupancy before enqueueing: `average = (1 - weight) * average + weight * occupancy`.
The drop probability is zero up to the minimum, increases linearly to the
configured probability between thresholds, and becomes one at the maximum.
Full queues are counted separately as overflow drops.

After a queue drains, its average decays by `(1 - weight) ^ idle_intervals` before
the next sample. An interval is the transmission duration of the incoming packet
at the current link bandwidth. Inspecting `earlyDropProbability` or
`getAverageQueueSize` never changes the state or consumes randomness.

History is independent per full-duplex direction and shared for half duplex.
New runs start with empty queues and zero history while retaining the RED
configuration. Changing policy, weight, thresholds or link mode resets history.
Live snapshots preserve history when the configuration is unchanged.

This is a deterministic simulation model with EWMA and idle decay. It does not
implement RED's optional packet-count correction or adaptive parameter tuning.

## Saved topologies and analysis

RED links accept an optional `red_queue_weight` JSON number. Omission defaults
to `1`, so existing topology files retain instantaneous RED. Saving emits the
configured weight. Invalid weights, including strings and null, are rejected.
The network analysis, intelligence context and chat snapshot include the weight.

For example, a link may contain:

```json
{
  "from": 0, "to": 1, "bandwidth": 10, "delay": 5, "loss": 0,
  "queue_capacity": 32, "queue_policy": "red",
  "red_min_threshold": 8, "red_max_threshold": 24,
  "red_max_drop_probability": 0.1, "red_queue_weight": 0.125
}
```

## Canvas editor

Use **Cable settings** to choose drop-tail or RED for new cables. RED exposes
minimum/maximum thresholds, maximum drop probability and queue weight. All
values are validated before adding a cable, so invalid drafts leave topology
and interfaces unchanged.

Right-click a cable to edit its queue policy. Draft values stay editable until
**Apply queue policy** is pressed; invalid drafts preserve the active policy.
Policy changes retain the cable identity and queued arrivals, and reset RED
history. The editor displays the current average separately for each independent
direction. Saving the topology preserves the complete policy.
