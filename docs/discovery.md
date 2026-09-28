# Live network discovery

KNS can follow topology snapshots produced by the sibling **KNS-Discovery** Go
collector. The collector reads active interfaces, default routes and the OS
neighbor cache on Windows or Linux. It does not scan ports or capture packets.

## Run on Windows

From the `KNS Environment` directory, build both applications:

```powershell
cmake -S KNS -B KNS/build
cmake --build KNS/build --config Release --parallel
Set-Location KNS-Discovery
go build -o bin/kns-discovery.exe ./cmd/kns-discovery
```

Start the collector from `KNS-Discovery` and leave it running:

```powershell
.\bin\kns-discovery.exe --output output/network.json --watch 5s
```

Once the first snapshot is published, open a second terminal in `KNS Environment`:

```powershell
.\KNS\build\app\Release\KNS.exe --watch-topology .\KNS-Discovery\output\network.json
```

For single-configuration builds, the executable is normally `KNS/build/app/KNS`
instead. `--watch-topology` requires GUI mode. To load a static snapshot, use
`--topology` or **Settings > Load Topology**. Enable **Follow topology file** in
Settings to follow a file loaded through the dialog; disable it to edit freely.

## Updates and editing

KNS checks the file approximately once per second, reading and validating on a
worker thread. It applies changed snapshots on the simulation thread. Unchanged
contents do not trigger reconciliation. Invalid, empty, missing or oversized
files leave the current graph intact and show an error in Settings; checking
continues so a corrected file can be loaded automatically. The live reader accepts
files up to 4 MiB, and topology snapshots support up to 4,096 node slots.

Discovery `external_id` values identify devices independently of snapshot-local
numeric IDs. Surviving devices retain simulator IDs; unchanged links retain their
IDs and queues. Updates preserve simulation time, pending events and TCP sessions.
Removed devices become inactive slots, which are never reused during that run;
a returning device receives a new slot. Existing sessions remain recorded, but
removing an endpoint or route can prevent their traffic from completing. Reload
the topology file if accumulated inactive slots reach the node limit.

The Topology panel supports adding/removing devices and links, changing device
labels/types and editing link metrics. Device colors and shapes distinguish types
in the Network panel; select a device to see its addresses, MAC, discovery evidence
and identity. Inactive devices cannot be selected for a new TCP connection.
Intelligence analysis is refreshed after topology changes.

## Interactive topology canvas

KNS opens an empty editable canvas when no topology file is supplied. The Network
panel includes a device palette with vector icons for computers, routers,
switches, wireless access points, servers, phones, printers, IoT devices, network
segments and unknown devices.

- Drag a palette icon onto the canvas to create a device, or click its tile and
  then click the desired location. Press Esc to cancel placement.
- **Select / Move** selects a device or cable. Drag a device to reposition it.
- **Cable** connects two clicked devices using a 100 Mbps, 1 ms full-duplex link.
  Existing connections are not duplicated by this tool.
- **TCP** selects a source and destination for a simulated TCP connection.
  Moving devices and creating cables never starts TCP traffic.
- Right-click a device to rename it, change its type, start a connection, or
  remove it. Right-click a cable to edit bandwidth, delay, loss and link state.
- **Delete selected** or Delete removes the selected device or cable. Device
  removal uses the simulator's existing tombstone and in-flight packet policy.
- The mouse wheel zooms around the pointer; the middle mouse button pans.
  **Fit** frames the topology, **Arrange** lays out active devices on a grid,
  and **Snap to grid** controls placement and movement snapping.

Save Topology As includes optional per-node `position: {"x": ..., "y": ...}`
coordinates. Loading or restarting preserves this layout. Old topology files
without coordinates are arranged automatically. Discovery snapshots that omit
coordinates preserve positions by device identity; snapshots with coordinates
apply those explicit positions. Merely moving a device does not change routing.
Live discovery still replaces network configuration, so disable Follow topology
file when adding or removing devices manually.

Use **Settings > Save Topology As...** to export the current edited graph to JSON.
Opening and saving use the operating system's file dialog: Windows Common Item
Dialog, macOS file chooser, or Zenity/KDialog on Linux (one must be installed).
The dialog confirms overwriting an existing file. Saving validates the snapshot and
replaces the destination only after the complete file has been written; failures
leave the previous file intact and appear in Settings. File paths support UTF-8,
including accented names. The saved graph preserves device metadata, inactive
node slots and link settings. It does not store simulation time, queued packets or
TCP sessions; saving leaves the running simulation intact.

Saving a copy does not change the file being followed. Disable **Follow topology
file** before making edits you want to keep, and save to a separate file from the
collector output. Load that saved file later to resume editing its graph.

While following a file, the next changed snapshot restores its graph configuration,
including overwriting manual edits. For persistent labels/types, use the collector's
`--inventory` file. Restarting the simulation retains the current graph and clears
simulation state; loading a topology file starts a new simulation from that file.

## Collector options and interpretation

```powershell
.\bin\kns-discovery.exe --interface "Wi-Fi" --watch 5s --inventory inventory.json --output output/network.json
```

`--interface` matches an exact OS interface name. `--timeout` defaults to `15s`.
The collector resolves observed device names by default, using up to eight
parallel DNS queries, a 750 ms query timeout and a three-second total budget.
Watch mode caches positive answers for ten minutes and negative answers for
one minute. `--resolve-names=false` disables this enrichment. Unresolved devices
keep their IP labels. Explicit hostname patterns can suggest device types;
`reverse_dns` and `hostname_hint` in the evidence identify these sources.
Gateway roles and manual inventory overrides take precedence over hostname hints.
`--bandwidth` (default `100` Mbps) and `--delay` (default `1` ms) configure simulation
assumptions. Without `--watch`, the collector publishes one snapshot and exits.
Stop a running collector with Ctrl+C.

In watch mode, collection failures are logged and retried at the configured
interval, including failures before the first snapshot. An existing snapshot is
preserved; if none exists yet, wait for the first successful publication before
opening it in KNS. Invalid static command-line options still fail immediately.

An inventory is a JSON object keyed by the exact `external_id` from a snapshot:

```json
{
  "host:example": {"label": "Workstation", "type": "computer"}
}
```

The collector reloads inventory on each collection; edits take effect without a
restart in watch mode. Use `{}` to clear overrides. If the inventory becomes
invalid or unavailable, the last published snapshot is preserved until corrected.
The supported types are `unknown`, `computer`, `router`, `switch`, `access_point`,
`server`, `phone`, `printer`, `iot` and `network_segment`. Types are descriptive
metadata; all devices share the simulator's TCP implementation.

A network segment represents inferred shared-network adjacency, not verified
cabling. A gateway is classified as a router because of its routing role. The
neighbor cache cannot reliably identify hidden switches, access points or hardware
models. An empty cache does not prove the absence of other devices. Bandwidth,
delay and loss are simulated assumptions, not network measurements. Generated
snapshots contain local network identifiers and belong in ignored output folders.

## Verification

```powershell
ctest --test-dir KNS/build -C Release --output-on-failure
Set-Location KNS-Discovery
go test ./...
go vet ./...
```

The C++ tests cover file errors/recovery, source switching, identity reconciliation,
device removal and preservation of sessions, simulation time, events and link
queues. The Go tests cover OS fixtures, deterministic snapshots, inventory overrides
and atomic publication.
