# Device roles

Device types select simulation capabilities. The core enforces the same rules
for GUI actions, CLI commands, direct C++ calls and loaded topology files.

| Type | Transit forwarding | HTTP/DNS client | HTTP host | DNS host | TCP initiate / listen |
| --- | --- | --- | --- | --- | --- |
| Computer | No | Yes | No | No | Yes / Yes |
| Server | No | Yes | Yes | Yes | Yes / Yes |
| Router | Yes | Yes (diagnostics) | No | Yes | Yes / No |
| Switch | Yes | No | No | No | No / No |
| Access point | Yes | No | No | No | No / No |
| Phone | No | Yes | No | No | Yes / Yes |
| Printer | No | No | Yes (status) | No | No / Yes |
| IoT | No | Yes | Yes (telemetry/status) | No | Yes / Yes |
| Network segment | Yes | No | No | No | No / No |
| Unknown (legacy graph node) | Yes | No | No | No | Yes / Yes |

These are KNS profiles, not claims about every real device's hardware or software.
Generic TCP endpoints support transport experiments; TCP listener capability does
not authorize hosting an HTTP or DNS application service. Printers accept modeled
TCP traffic and can expose configured HTTP status pages; IoT nodes expose configured
telemetry pages. This does not add a print spooler, sensors or autonomous telemetry.

## Network behavior

Routing can terminate at any active node, but only infrastructure nodes (or legacy
unknown nodes) may occur between the source and destination. A computer connected
to two cables does not become a router. The rule applies to all four routing
metrics and to actual packet forwarding, including packets already in flight when
a transit device changes type. Routes refresh automatically after a type change.

Switches, access points and segments provide transit connectivity in KNS's current
graph model. This change does not introduce Ethernet MAC learning, VLANs, spanning
tree, Wi-Fi association, IP subnet forwarding or a distinct L2/L3 packet stack.
Routers additionally support the modeled DNS service and client diagnostics.

Automatic workloads on typed networks choose one reachable TCP peer per endpoint,
preferring servers in stable device-ID order. They no longer open a session for
every cable or use switches/APs as application endpoints. Legacy untyped topology
workloads preserve the existing per-link TCP experiment behavior.

## GUI and CLI

Select a device to see its role in **Device Services**. Unsupported service/client
actions are disabled. The protocol selector only offers service kinds supported by
the current device. The TCP panel and canvas reject incompatible client/listener
pairs. The type editor reports an error if existing services conflict with a new
role; remove those services before changing the type.

The per-device CLI `help` lists operations supported by that role. Commands entered
manually still go through core validation. `show role`, `show interfaces` and
`show routes` inspect a device without generating application traffic. Routing
inspection on a non-forwarding endpoint describes its own outgoing traffic.

Examples:

```text
# On a server
service add http web 80
service page web / 200 "Hello"

# On a computer
http get 2 80 /

# On a router
service add dns names 53
service record names web.example 192.0.2.20

# On a switch (read-only diagnostics)
show role
show interfaces
show routes
```

The comments above are explanatory; enter the commands themselves in the device CLI.

## Loading, discovery and live edits

Topology JSON with services unsupported by its node type is rejected. A node with
no type is `unknown` and must be assigned a suitable explicit type before hosting
application services. For files created before role enforcement, update the node's
`type` to the intended profile (for example `server` for HTTP/DNS) before loading;
configuration is never silently deleted or promoted to a different role.

Discovery snapshots that omit services preserve existing local configuration. If
an incoming type conflicts with retained services, synchronization fails before
mutating the topology. An explicit services array permits replacing the type and
services together, including `[]` to clear old services.

Type changes invalidate delayed application replies and prevent clients that lost
their capabilities from accepting pending results. Existing TCP sessions with an
incompatible source or destination are canceled at the next simulation step, with
listener backlog slots released. In-flight packets still release their link queues.
Existing listeners cannot accept while their device's role forbids listening.
