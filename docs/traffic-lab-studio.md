# Traffic Lab in the desktop application

The **Traffic Lab** window prepares bounded concurrent abstract workloads on the
current topology. It uses the same native `TrafficRun` model and plan format as
the [headless Traffic Lab](traffic-models.md), with live per-flow results.

## Run an experiment

1. Load a topology with active source/destination devices and a suitable route.
   `app/topologies/parallel-cables-lab.json` is one example; use Client #0 to
   Application server #2 for a two-hop path.
2. If traffic is already scheduled or has run, click **Restart** in Stats.
   Restart preserves the current topology and clears the previous run.
3. Select **Traffic Lab** in the right-hand dock. Set source/destination IDs,
   wire bytes, start time, duration and pattern. Times are virtual. Duration is
   the source-generation window; packets may arrive after it ends.
4. Use **Add flow** to duplicate the selected draft, then edit its endpoints or
   schedule. The editor supports 1..16 concurrent flows with IDs assigned in list
   order. Removing a draft renumbers subsequent flows and changes their derived streams.
5. Set a traffic seed and global packet budget, then click **Schedule workload**.
   The simulation is paused. Use **Resume** in Stats to run it, or **Step** to
   dispatch one event. Scheduled configuration becomes read-only and collapses
   to make space for results; expand **Workload configuration** to inspect it.

The GUI requires a fresh engine with no scheduled events, TCP sessions or prior
transmissions/execution. It does not silently replace an existing workload.
Restart is required for a second plan. Draft settings remain available across
restarts, so check endpoint IDs after loading a different topology. The native
validator rejects nonexistent or removed endpoints and invalid values before
claiming a plan or scheduling any workload events.

## Patterns and limits

| Pattern | Editor controls | Meaning |
| --- | --- | --- |
| Periodic | Interval in ms | First message at start, then fixed intervals |
| CBR | Offered rate in Mbps | Interval is `8 * wire_bytes / rate_bps` |
| Burst | Packets per burst, burst interval and packet spacing in ms | Fixed nonoverlapping source bursts |
| Poisson | Packets per second | Exponentially sampled interarrival times, including the first wait |

The source end time is exclusive. The optional packet deadline is measured from
each message's generation time. CBR rate is offered wire load, not guaranteed
throughput. The traffic seed controls flow schedules; the separately displayed
network seed controls network randomness. The native limits include 1..1,000,000
generated packets globally, 1..1,048,576 wire bytes per message and a generation
window ending no later than 1,000,000 virtual seconds. Invalid drafts remain
editable and create no workload events. All counts are scoped to this workload.

The editor currently creates one-way abstract messages. These are not TCP,
HTTP or real sockets, and do not require matching application services. Active
endpoints are required; intermediate forwarding still follows device roles.
Native uniform/custom/replay schedules and application response abstractions
remain available through the existing headless plan interface.

## Results and reuse

The panel shows generated, delivered, dropped and pending counts, plus per-flow
deadline outcomes. `--` means a flow has no packet deadline. **Workload drained**
means no source generation or in-flight workload messages remain, and may include
losses or missed deadlines. **Packet budget reached** is explicitly a partial
workload, even after its admitted messages drain. Cancellation/failure is marked
interrupted. Use assertions in the existing validation tooling for acceptance
criteria. Restarting or replacing the engine clears the panel's result attachment.
Sparse Poisson workloads can sample their first message beyond the source window,
producing no events. The GUI marks this zero-message workload finished immediately.

**Copy plan JSON** validates an unscheduled draft without changing the engine.
After scheduling, it copies the original configuration, including if the live
topology subsequently changes. Save the clipboard text as a JSON file and pass
it to `--traffic-plan` for a headless run on an equivalent topology/build.
**Copy report JSON** copies the native workload report with the network seed and
explicit interruption state. These buttons are explicit user actions; the panel
does not automatically write files or overwrite existing evidence.
