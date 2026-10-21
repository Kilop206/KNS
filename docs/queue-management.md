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
