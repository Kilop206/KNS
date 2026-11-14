#!/usr/bin/env python3
"""Generate/run a bounded industrial telemetry and control workload (no MQTT claim)."""
import argparse
import copy
import json
from pathlib import Path
import platform
import sys
import time

import experiment_profile
import traffic_config


def configuration(sensors, redundant=False, seed=42):
    traffic_config.integer(sensors, 1, 250, "sensors")
    traffic_config.integer(seed, 0, 2**64 - 1, "seed")
    if type(redundant) is not bool:
        raise ValueError("redundant must be a boolean")
    gateway, server, actuator, plc = sensors, sensors + 1, sensors + 2, sensors + 3
    nodes = [{"id": index, "type": "iot", "label": f"Sensor {index + 1}"} for index in range(sensors)]
    nodes += [{"id": gateway, "type": "router", "label": "Industrial gateway"},
              {"id": server, "type": "server", "label": "Telemetry destination"},
              {"id": actuator, "type": "iot", "label": "Actuator abstraction"},
              {"id": plc, "type": "iot", "label": "PLC abstraction"}]
    def link(a, b, bandwidth=100, delay=1, capacity=8):
        return {"from": a, "to": b, "bandwidth": bandwidth, "delay": delay,
                "loss": 0, "queue_capacity": capacity, "mode": "full_duplex"}
    # Stable index 0 is the primary uplink targeted by the fault plan.
    links = [link(gateway, server, 2, 5, 32)]
    links += [link(index, gateway) for index in range(sensors)]
    links += [link(plc, gateway), link(actuator, gateway)]
    if redundant:
        standby = len(nodes)
        nodes.append({"id": standby, "type": "router", "label": "Alternate uplink router"})
        links += [link(gateway, standby, 2, 5, 32), link(standby, server, 2, 5, 32)]
    topology = {"schema_version": "1.0", "name": f"Industrial telemetry with {sensors} sensors", "nodes": nodes, "links": links}
    flows = [{"id": index + 1, "source": index, "destination": server,
              "start_s": (index % 10) / 100, "end_s": 1, "wire_bytes": 125,
              "priority": 3, "deadline_s": 0.03, "schedule": {"kind": "periodic", "interval_s": 0.1},
              "application": {"kind": "telemetry"}} for index in range(sensors)]
    flows.append({"id": sensors + 1, "source": plc, "destination": actuator,
        "start_s": 0, "end_s": 1, "wire_bytes": 64, "priority": 7, "deadline_s": 0.01,
        "schedule": {"kind": "periodic", "interval_s": 0.2},
        "application": {"kind": "request_response", "response_wire_bytes": 64, "processing_delay_s": 0.001, "deadline_s": 0.02}})
    plan = {"schema_version": "1.0", "seed": seed, "max_packets": 20000, "flows": flows}
    traffic_config.validate(plan, topology)
    fault = {"schema_version": "1.0", "in_flight_policy": "complete_accepted_hops",
        "probes": [{"source": 0, "destination": server}], "observe_at": [0, 0.29, 0.31, 0.49, 0.51, 1.1],
        "events": [{"at": 0.3, "type": "link_down", "link_index": 0}, {"at": 0.5, "type": "link_up", "link_index": 0}]}
    profile = {"schema_version": "1.0", "name": f"industrial-{sensors}", "index": 0,
        "traffic_plan": plan, "seeds": [seed], "max_cases": 3,
        "analytics": {"schema_version": "1.0", "until_s": 1.1, "interval_s": 0.01,
                      "max_samples": 128, "max_groups": 128, "group_retention": 16},
        "experiments": [{"name": "control", "bandwidth": 2},
                        {"name": "uplink-outage", "bandwidth": 2, "fault_plan": fault},
                        {"name": "congestion", "bandwidth": 0.5}]}
    return topology, profile


def save(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sensors", type=int, default=100, help="1..250 explicit sensor flows; larger targets remain pending")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--redundant", action="store_true", help="Alternate uplink path; the common gateway remains a single point of failure")
    parser.add_argument("--output", type=Path, required=True, help="New scenario directory")
    parser.add_argument("--exe", type=Path, help="If supplied, execute all three variants and save measured results")
    args = parser.parse_args(argv)
    try:
        topology, profile = configuration(args.sensors, args.redundant, args.seed)
        if args.exe and not args.exe.is_file():
            raise ValueError("KNS executable does not exist")
        output = args.output.resolve()
        profile["topology"] = str(output / "topology.json")
        experiment_profile.validate_profile(copy.deepcopy(profile), topology)
        output.mkdir(parents=True, exist_ok=False)
        save(output / "topology.json", topology)
        save(output / "traffic.json", profile["traffic_plan"])
        save(output / "profile.json", profile)
        save(output / "model.json", {"schema_version": "1.0", "sensor_count": args.sensors, "node_count": len(topology["nodes"]),
            "link_count": len(topology["links"]), "alternate_uplink": args.redundant,
            "model": "IoT wire telemetry and abstract PLC/actuator request-response; fixed processing timer; no MQTT or physical process",
            "limitations": "250-sensor generator limit; gateway/broker node failures, 1000/10000 targets, MQTT QoS and CPU capacity remain pending; priority is metadata"})
        if args.exe:
            started = time.perf_counter()
            code = experiment_profile.main(["--profile", str(output / "profile.json"), "--exe", str(args.exe.resolve()),
                                           "--output", str(output / "experiment")])
            elapsed = time.perf_counter() - started
            if code:
                return code
            benchmark = json.loads((output / "experiment/runs/benchmark.json").read_text())
            measurements = []
            for case in benchmark["cases"]:
                report = json.loads(Path(case["traffic_report"]).read_text())
                measurements.append({"case_id": case["case_id"], "status": case["status"],
                    "wall_clock_duration_s": case["wall_clock_duration_s"], "simulation_time_s": report["simulation_time_s"],
                    "generated": report["generated"], "delivered": report["delivered"], "dropped": report["dropped"],
                    "sensor_delivered_on_time": sum(f["delivered_on_time"] for f in report["flows"] if f["id"] <= args.sensors),
                    "control_operations_completed": report["flows"][-1]["operations_completed"]})
            save(output / "measurements.json", {"schema_version": "1.0", "sensor_count": args.sensors, "platform": platform.platform(),
                "python_version": platform.python_version(), "total_runner_wall_s": elapsed, "cases": measurements,
                "methodology": "Measured wall time includes runner, simulation and exports; not a hardware-independent performance threshold; binary/config identities are in the experiment manifest; memory/event-rate not measured"})
        print("Industrial scenario: " + str(output))
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("[ERROR] " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
