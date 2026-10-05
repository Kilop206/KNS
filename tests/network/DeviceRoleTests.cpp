#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "network/TopologyLoader.hpp"
#include "network/services/DeviceCLI.hpp"

using namespace kns;
namespace {
void role(Topology& topology, int id, DeviceType type) {
    auto device = topology.getNode(id)->getDeviceInfo();
    device.type = type;
    topology.setNodeDeviceInfo(id, std::move(device));
}
NetworkService service(ServiceKind kind) {
    NetworkService result;
    result.kind = kind;
    result.name = serviceKindName(kind);
    result.port = kind == ServiceKind::Http ? 80 : 53;
    return result;
}
Topology chain(DeviceType middle) {
    Topology topology(3);
    role(topology, 0, DeviceType::Computer);
    role(topology, 1, middle);
    role(topology, 2, DeviceType::Server);
    auto http = service(ServiceKind::Http);
    http.pages["/"] = {200, "server response"};
    topology.setNodeServices(2, {http});
    topology.addLink(0, 1, 100, 1, 0);
    topology.addLink(1, 2, 100, 1, 0);
    return topology;
}
}

TEST_CASE("Device profiles enforce service hosting and client capabilities through core APIs", "[roles][services]") {
    struct Profile { DeviceType type; bool http, dns, client; };
    for (const auto profile : {
        Profile{DeviceType::Unknown, false, false, false},
        Profile{DeviceType::Computer, false, false, true},
        Profile{DeviceType::Router, false, true, true},
        Profile{DeviceType::Switch, false, false, false},
        Profile{DeviceType::AccessPoint, false, false, false},
        Profile{DeviceType::Server, true, true, true},
        Profile{DeviceType::Phone, false, false, true},
        Profile{DeviceType::Printer, true, false, false},
        Profile{DeviceType::IoT, true, false, true},
        Profile{DeviceType::NetworkSegment, false, false, false}}) {
        INFO(toString(profile.type));
        Topology topology(2);
        role(topology, 0, profile.type);
        role(topology, 1, DeviceType::Server);
        for (auto kind : {ServiceKind::Http, ServiceKind::Dns}) {
            const bool allowed = kind == ServiceKind::Http ? profile.http : profile.dns;
            if (allowed) REQUIRE_NOTHROW(topology.setNodeServices(0, {service(kind)}));
            else REQUIRE_THROWS_AS(topology.setNodeServices(0, {service(kind)}), std::invalid_argument);
            topology.setNodeServices(0, {});
        }
        SimulationEngine engine(topology);
        if (profile.client) REQUIRE_NOTHROW(engine.networkServices().request(engine, 0, 1, ServiceKind::Http, 80, "/"));
        else {
            REQUIRE_THROWS_AS(engine.networkServices().request(engine, 0, 1, ServiceKind::Http, 80, "/"), std::invalid_argument);
            REQUIRE_FALSE(engine.hasEvents());
            REQUIRE(engine.networkServices().requests().empty());
        }
    }
}

TEST_CASE("Only network infrastructure forwards traffic between endpoints for every metric", "[roles][routing]") {
    for (auto type : {DeviceType::Unknown, DeviceType::Router, DeviceType::Switch, DeviceType::AccessPoint, DeviceType::NetworkSegment}) {
        INFO(toString(type));
        SimulationEngine engine(chain(type));
        for (auto metric : {RoutingMetric::Delay, RoutingMetric::Bandwidth, RoutingMetric::HopCount, RoutingMetric::DelayBandwidth}) {
            engine.setRoutingMetric(metric);
            REQUIRE(engine.traceRoute(0, 2).status == RouteStatus::Reachable);
        }
        const auto id = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/");
        engine.run();
        REQUIRE(engine.networkServices().requests().at(id).response == "server response");
    }
    for (auto type : {DeviceType::Computer, DeviceType::Server, DeviceType::Phone, DeviceType::Printer, DeviceType::IoT}) {
        INFO(toString(type));
        SimulationEngine engine(chain(type));
        for (auto metric : {RoutingMetric::Delay, RoutingMetric::Bandwidth, RoutingMetric::HopCount, RoutingMetric::DelayBandwidth}) {
            engine.setRoutingMetric(metric);
            REQUIRE(engine.traceRoute(0, 2).status == RouteStatus::Unreachable);
            REQUIRE(engine.traceRoute(0, 1).status == RouteStatus::Reachable);
            REQUIRE(engine.traceRoute(1, 2).status == RouteStatus::Reachable);
        }
    }
}

TEST_CASE("Routing avoids cheap endpoint shortcuts and reacts to live role changes", "[roles][routing]") {
    auto topology = chain(DeviceType::Computer);
    const int router = topology.addNode();
    role(topology, router, DeviceType::Router);
    topology.addLink(0, router, 10, 20, 0);
    topology.addLink(router, 2, 10, 20, 0);
    SimulationEngine engine(topology);
    REQUIRE(engine.getNextHop(0, 2) == router);
    role(engine.getTopology(), 1, DeviceType::Switch);
    REQUIRE(engine.getNextHop(0, 2) == 1);
    role(engine.getTopology(), 1, DeviceType::Phone);
    REQUIRE(engine.getNextHop(0, 2) == router);
}

TEST_CASE("An in-flight packet cannot use an endpoint as a transit hop after retyping", "[roles][services]") {
    SimulationEngine engine(chain(DeviceType::Router));
    const auto id = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/");
    role(engine.getTopology(), 1, DeviceType::Computer);
    engine.run();
    REQUIRE(engine.networkServices().requests().at(id).state == ServiceRequestState::TimedOut);
    REQUIRE(engine.getStats().packets_lost == 1);
    REQUIRE(engine.getPacketsInTransit().empty());
    REQUIRE(engine.validateSimulation().passed());
}

TEST_CASE("Infrastructure and printer TCP roles are enforced before sessions or listeners exist", "[roles][tcp]") {
    Topology topology(3);
    role(topology, 0, DeviceType::Computer);
    role(topology, 2, DeviceType::Server);
    for (auto type : {DeviceType::Router, DeviceType::Switch, DeviceType::AccessPoint, DeviceType::NetworkSegment}) {
        INFO(toString(type));
        role(topology, 1, type);
        SimulationEngine engine(topology);
        REQUIRE_THROWS_AS(engine.createTCPSession(0, 1), std::invalid_argument);
        REQUIRE_THROWS_AS(engine.startTCPListen(1), std::invalid_argument);
        REQUIRE_FALSE(engine.hasListener(1));
        REQUIRE(engine.acceptOnListener(1, 0, 10) == TCPListener::INVALID_SESSION_ID);
        if (type == DeviceType::Router) REQUIRE_NOTHROW(engine.createTCPSession(1, 2));
        else {
            REQUIRE_THROWS_AS(engine.startTCPConnection(1, 2), std::invalid_argument);
            REQUIRE(engine.getTCPSessions().empty());
            REQUIRE_FALSE(engine.hasEvents());
        }
    }
    role(topology, 1, DeviceType::Printer);
    SimulationEngine printer(topology);
    REQUIRE_THROWS_AS(printer.startTCPConnection(1, 2), std::invalid_argument);
    REQUIRE_NOTHROW(printer.startTCPListen(1));
    REQUIRE_NOTHROW(printer.createTCPSession(0, 1));
}

TEST_CASE("Retyping a TCP endpoint disables existing listeners and cancels incompatible sessions", "[roles][tcp]") {
    SimulationEngine engine(chain(DeviceType::Switch));
    engine.startTCPListen(2);
    engine.startTCPConnection(0, 2);
    // Remove application configuration before changing the server to an infrastructure role.
    engine.getTopology().setNodeServices(2, {});
    role(engine.getTopology(), 2, DeviceType::Switch);
    REQUIRE_FALSE(engine.hasListener(2));
    REQUIRE_FALSE(engine.hasListener(2, 0));
    REQUIRE(engine.acceptOnListener(2, 0, 20) == TCPListener::INVALID_SESSION_ID);
    engine.run();
    REQUIRE(engine.getTCPSessions().empty());
    REQUIRE(engine.getPacketsInTransit().empty());
}

TEST_CASE("Incompatible type edits and JSON service configurations are rejected without mutation", "[roles][save]") {
    auto topology = chain(DeviceType::Switch);
    const auto before = TopologyLoader::toJson(topology);
    REQUIRE_THROWS_AS(role(topology, 2, DeviceType::Computer), std::invalid_argument);
    REQUIRE(TopologyLoader::toJson(topology) == before);
    auto document = before;
    document["nodes"][2]["type"] = "switch";
    REQUIRE_THROWS(TopologyLoader::fromJson(document));
    document["nodes"][2]["services"] = nlohmann::json::array();
    REQUIRE_NOTHROW(TopologyLoader::fromJson(document));
    Node node(0);
    REQUIRE_THROWS_AS(node.setServices({service(ServiceKind::Http)}), std::invalid_argument);
}

TEST_CASE("Discovery role changes validate retained services before mutating the topology", "[roles][discovery]") {
    auto topology = chain(DeviceType::Switch);
    auto document = TopologyLoader::toJson(topology);
    document["nodes"][0]["label"] = "must not partially change";
    document["nodes"][2]["type"] = "switch";
    document["nodes"][2].erase("services");
    const auto snapshot = TopologyLoader::fromJson(document);
    const auto before = TopologyLoader::toJson(topology);
    REQUIRE_THROWS(topology.synchronizeFrom(snapshot));
    REQUIRE(TopologyLoader::toJson(topology) == before);
    document["nodes"][2]["services"] = nlohmann::json::array();
    REQUIRE(topology.synchronizeFrom(TopologyLoader::fromJson(document)));
    REQUIRE(topology.getNode(2)->getDeviceInfo().type == DeviceType::Switch);
    REQUIRE(topology.getNode(2)->getServices().empty());
}

TEST_CASE("Device CLI advertises only supported operations and blocks manually entered commands", "[roles][cli]") {
    SimulationEngine engine(chain(DeviceType::Switch));
    REQUIRE(DeviceCLI::execute(engine, 1, "help").output.find("service add") == std::string::npos);
    REQUIRE_FALSE(DeviceCLI::execute(engine, 1, "service add http web 80").ok);
    REQUIRE_FALSE(DeviceCLI::execute(engine, 1, "http get 2 80 /").ok);
    REQUIRE_FALSE(DeviceCLI::execute(engine, 0, "service add dns names 53").ok);
    REQUIRE(DeviceCLI::execute(engine, 0, "help").output.find("http get") != std::string::npos);
    REQUIRE(DeviceCLI::execute(engine, 1, "show role").output.find("Switch:") != std::string::npos);
    REQUIRE(DeviceCLI::execute(engine, 1, "show routes").output.find("destination=2") != std::string::npos);
    REQUIRE(DeviceCLI::execute(engine, 1, "show interfaces").output.find("peer=0") != std::string::npos);
    REQUIRE(DeviceCLI::execute(engine, 2, "service add dns names 53").ok);
}
