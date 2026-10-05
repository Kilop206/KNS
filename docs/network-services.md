# Device network services

KNS devices can host configurable HTTP and DNS application services. Both the
GUI and the device CLI edit the same configuration, saved with the topology.
Open `app/topologies/services-lab.json` for a client, router and server example.
Service availability follows [device roles](device-roles.md): servers host HTTP
and DNS, routers host DNS, printers/IoT expose HTTP, and computers/phones are clients.
Switches, APs and passive segments only forward traffic.

## GUI

Select a device on the topology canvas and open **Device Services**:

- **Services**: add an HTTP or DNS service, select it, start/stop it, change its
  port and processing delay (click **Apply settings** or press Enter), and add, edit or remove entries.
  HTTP entries contain a path, status and text body. DNS entries contain a name
  and IPv4 address. Select an existing entry to edit it.
- **Client**: enter a destination device ID and port, then send an HTTP GET or
  DNS A query. **Resume** or **Step** advances the simulation. Results show protocol
  status, response and elapsed simulated time. HTTP uses 404 for unknown paths;
  DNS uses 0 for NOERROR and 3 for NXDOMAIN.
- **CLI**: execute the commands below in the selected device's context. Terminal
  history is separate per device. `show requests` displays asynchronous results.

**Settings > Save topology** persists services alongside nodes and links.
Restart keeps configuration but clears requests, timers and terminal history.
Discovery snapshots without a `services` property preserve local configuration;
an explicit `"services": []` clears it.

## Device CLI

Use the GUI terminal or open a console without a graphical window:

```powershell
.\build\app\KNS.exe --topology app\topologies\services-lab.json --device-cli 0
```

Configure a server and query it from another device:

```text
device 2
service add http api 8080
service page api /health 200 "service is healthy"
service delay api 25
service record names api.example 192.0.2.20
show services
device 0
http get 2 8080 /health
dns query 2 53 API.Example.
run
show requests
save "my-services.json"
exit
```

The GUI CLI provides these commands:

```text
help
show role
show interfaces
show routes
show services
show requests
service add <http|dns> <name> <port>
service remove <name>
service start <name>
service stop <name>
service delay <name> <milliseconds>
service page <name> <path> <status> "body"
service unpage <name> <path>
service record <name> <hostname> <IPv4>
service unrecord <name> <hostname>
http get <device-id> <port> <path>
dns query <device-id> <port> <hostname>
```

The stdin console additionally supports `device`, `run`, `save` and `exit`.
`help` filters service and client commands according to the selected device's role.
Input can be piped from a text file for repeatable labs. Invalid commands print
an error, preserve configuration, and make the console exit with status 1.
Timeouts and HTTP/DNS error responses are simulation results, not command errors.
Quoted arguments accept escaped quotes and backslashes; host shell commands are
never executed. In the GUI, use Resume/Step and Settings to advance or save.

## Simulation scope

HTTP models GET path lookup, configurable status codes (200–599) and text bodies;
DNS models case-insensitive A record lookup with trailing-dot normalization.
DNS addresses are returned as configured data: clients target device IDs, so a
DNS answer does not automatically change topology addressing or resolve HTTP URLs.

These are application-level simulations using KNS service datagrams, **not
wire-compatible HTTP/TCP or DNS/UDP implementations**. No host sockets, real web
server, TLS, HTTP parsing, browser rendering, DNS recursion/cache/TTL, or TCP
handshake/retransmission is involved. Each request and response travels hop by
hop through the actual simulated routing, link queues, bandwidth, delay and loss
machinery, and appears in packet visualization and aggregate packet statistics.
The modeled packet size is 28 bytes plus text payload; this is an approximation,
not HTTP/DNS wire encoding. Existing TCP experiments are independent.

Absent/stopped services, lost packets and missing return routes time out after
five simulated seconds (configurable through the core API). Local queries also
work. Response latency includes both paths and server processing delay. Successful
replies cancel their timeout events; expired requests cancel pending server
processing. Editing a service invalidates only that service's queued processing
replies; other services remain unaffected. Packets already transmitted continue through the network. Deleting an
endpoint prevents new responses, and late replies cannot overwrite a timeout.

Limits: 32 services per device, unique names and protocol/port pairs, ports
1–65535, 128 entries per service, 4096-byte HTTP bodies, 512-byte requests,
processing delay 0–60000 ms, and 256 retained requests per engine. Completed
requests are evicted first; 256 pending requests reject additional work. Names,
addresses and JSON numeric fields are validated before configuration changes.

## Core integration

`NetworkService` defines configuration; `Topology::setNodeServices` validates
atomic updates; `ServiceRuntime` owns per-engine requests and scheduled responses;
`DeviceCLI` is shared by both front ends. Topology cloning copies configuration
without sharing mutable service state, and JSON loading remains compatible with
topologies that omit services.

Run focused tests with `build/tests/kns_tests "[services]"` and the full suite with
`ctest --test-dir build --output-on-failure`.
