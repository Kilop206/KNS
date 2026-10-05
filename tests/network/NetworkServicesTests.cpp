#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "network/TopologyLoader.hpp"
#include "network/services/DeviceCLI.hpp"

#include <limits>

using namespace kns;
namespace {
Topology serviceTopology() {
    Topology topology(3);
    DeviceInfo client, router, server;
    client.type = DeviceType::Computer;
    router.type = DeviceType::Router;
    server.type = DeviceType::Server;
    topology.setNodeDeviceInfo(0, client);
    topology.setNodeDeviceInfo(1, router);
    topology.setNodeDeviceInfo(2, server);
    topology.addLink(0, 1, 100, 10, 0);
    topology.addLink(1, 2, 100, 10, 0);
    NetworkService http;
    http.name = "web";
    http.delay_ms = 25;
    http.pages["/"] = {200, "Hello KNS"};
    NetworkService dns;
    dns.name = "names";
    dns.kind = ServiceKind::Dns;
    dns.port = 53;
    dns.records["web.example"] = "192.0.2.20";
    topology.setNodeServices(2, {http, dns});
    return topology;
}
}

TEST_CASE("Completed service requests do not keep an idle simulation running until timeout", "[services][audit]") {
    SimulationEngine engine(serviceTopology());
    const auto id = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/");
    engine.run();
    REQUIRE(engine.now() == engine.networkServices().requests().at(id).finished_at);
}

TEST_CASE("DNS records reject signed IPv4 octets atomically", "[services][audit]") {
    SimulationEngine engine(serviceTopology());
    const auto before = engine.getTopology().getNode(2)->getServices();
    for (const auto* address : {"-1.2.3.4", "1.-2.3.4", "1.2.-0.4", "1.2.3.-1", "+1.2.3.4"}) {
        INFO(address);
        REQUIRE_FALSE(DeviceCLI::execute(engine, 2, std::string("service record names bad.example ") + address).ok);
        REQUIRE(engine.getTopology().getNode(2)->getServices() == before);
    }
}

TEST_CASE("Timed out service processing never emits a late response", "[services][audit]") {
    SimulationEngine engine(serviceTopology());
    const auto id = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/", 0.04);
    engine.run();
    REQUIRE(engine.networkServices().requests().at(id).state == ServiceRequestState::TimedOut);
    REQUIRE(engine.getStats().packets_sent == 1);
    REQUIRE(engine.now() == 0.04);
}

TEST_CASE("Editing DNS does not cancel a different service's pending HTTP response", "[services][audit]") {
    SimulationEngine engine(serviceTopology());
    const auto id = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/");
    engine.processEvent();
    engine.processEvent();
    REQUIRE(DeviceCLI::execute(engine, 2, "service record names other.example 192.0.2.10").ok);
    engine.run();
    REQUIRE(engine.networkServices().requests().at(id).state == ServiceRequestState::Complete);
}

TEST_CASE("HTTP and DNS services traverse routed links and return measured responses", "[services]") {
    SimulationEngine engine(serviceTopology());
    const auto http = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/");
    const auto dns = engine.networkServices().request(engine, 0, 2, ServiceKind::Dns, 53, "WEB.Example.");
    REQUIRE(engine.networkServices().requests().at(http).state == ServiceRequestState::Pending);
    REQUIRE_FALSE(engine.getPacketsInTransit().empty());
    engine.run();
    const auto& result = engine.networkServices().requests().at(http);
    REQUIRE(result.state == ServiceRequestState::Complete);
    REQUIRE(result.status == 200);
    REQUIRE(result.response == "Hello KNS");
    REQUIRE(result.finished_at >= 0.065);
    REQUIRE(result.finished_at < 0.066);
    REQUIRE(engine.networkServices().requests().at(dns).response == "192.0.2.20");
    REQUIRE(engine.networkServices().requests().at(dns).status == 0);
    REQUIRE(engine.getStats().packets_sent == 4);
    REQUIRE(engine.getStats().packets_delivered == 4);
    REQUIRE(engine.getTCPSessions().empty());
    REQUIRE(engine.validateSimulation().passed());
}

TEST_CASE("HTTP missing pages and DNS missing names produce protocol results", "[services]") {
    SimulationEngine engine(serviceTopology());
    const auto http = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/missing");
    const auto dns = engine.networkServices().request(engine, 0, 2, ServiceKind::Dns, 53, "missing.example");
    engine.run();
    REQUIRE(engine.networkServices().requests().at(http).status == 404);
    REQUIRE(engine.networkServices().requests().at(dns).status == 3);
    REQUIRE(engine.networkServices().requests().at(dns).response == "NXDOMAIN");
}

TEST_CASE("Unavailable services and broken network paths time out", "[services]") {
    auto topology = serviceTopology();
    int port = 80;
    SECTION("stopped service") {
        auto services = topology.getNode(2)->getServices();
        services[0].enabled = false;
        topology.setNodeServices(2, services);
    }
    SECTION("wrong port") { port = 8080; }
    SECTION("disconnected") { topology.setLinkUp(0, 1, false); }
    SECTION("complete packet loss") { topology.setGlobalLossProb(1.0); }
    SECTION("one way path") {
        topology.removeLink(1, 2);
        topology.addLink(1, 2, 100, 10, 0, LinkMode::SIMPLEX);
    }
    SimulationEngine engine(topology);
    const auto id = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, port, "/", 0.1);
    engine.run();
    REQUIRE(engine.networkServices().requests().at(id).state == ServiceRequestState::TimedOut);
    REQUIRE(engine.networkServices().requests().at(id).finished_at == Catch::Approx(0.1));
    REQUIRE(engine.getPacketsInTransit().empty());
    for (const auto& link : engine.getTopology().getLinks()) REQUIRE(link->getQueueSize() == 0);
}

TEST_CASE("Service replies respect deletion restart and timeout during processing", "[services]") {
    SimulationEngine engine(serviceTopology());
    const auto id = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/", 0.1);
    REQUIRE(engine.processEvent()); // first hop
    REQUIRE(engine.processEvent()); // server schedules reply
    SECTION("server deleted") { engine.deleteNode(2); }
    SECTION("client deleted") { engine.deleteNode(0); }
    SECTION("service stopped and restarted") {
        REQUIRE(DeviceCLI::execute(engine, 2, "service stop web").ok);
        REQUIRE(DeviceCLI::execute(engine, 2, "service start web").ok);
    }
    SECTION("service removed") { REQUIRE(DeviceCLI::execute(engine, 2, "service remove web").ok); }
    SECTION("return link fails") { engine.toggleLinkUp(1, 2, false); }
    engine.run();
    REQUIRE(engine.networkServices().requests().at(id).state == ServiceRequestState::TimedOut);
    REQUIRE(engine.getPacketsInTransit().empty());
}

TEST_CASE("Late replies cannot overwrite a timeout and local queries are supported", "[services]") {
    SimulationEngine engine(serviceTopology());
    const auto late = engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/", 0.01);
    const auto local = engine.networkServices().request(engine, 2, 2, ServiceKind::Dns, 53, "web.example");
    engine.run();
    REQUIRE(engine.networkServices().requests().at(late).state == ServiceRequestState::TimedOut);
    REQUIRE(engine.networkServices().requests().at(local).state == ServiceRequestState::Complete);
    REQUIRE(engine.networkServices().requests().at(local).response == "192.0.2.20");
    REQUIRE(engine.validateSimulation().passed());
}

TEST_CASE("Device commands configure services atomically and reject malformed inputs", "[services][cli]") {
    SimulationEngine engine(Topology(2));
    DeviceInfo server;
    server.type = DeviceType::Server;
    engine.getTopology().setNodeDeviceInfo(1, server);
    REQUIRE(DeviceCLI::execute(engine, 1, "service add http web 8080").ok);
    REQUIRE(DeviceCLI::execute(engine, 1, R"(service page web / 201 "hello world")").ok);
    REQUIRE(DeviceCLI::execute(engine, 1, "service add dns names 53").ok);
    REQUIRE(DeviceCLI::execute(engine, 1, "service record names WEB.Example. 192.0.2.1").ok);
    const auto before = TopologyLoader::toJson(engine.getTopology());
    for (const auto& command : {"service add http duplicate 8080", "service add http web 80", "service add dns bad 65536",
        "service page web / 200.5 hi", "service page web / 999 hi", "service page names / 200 hi",
        "service record names invalid.example 999.0.0.1", "service record names -bad.example 1.2.3.4",
        "service delay web -1", "service stop web extra", "service remove missing", "service page web / 200 \"unterminated"}) {
        INFO(command);
        REQUIRE_FALSE(DeviceCLI::execute(engine, 1, command).ok);
        REQUIRE(TopologyLoader::toJson(engine.getTopology()) == before);
    }
    REQUIRE(DeviceCLI::execute(engine, 1, "show services").output.find("hello world") != std::string::npos);
    REQUIRE(DeviceCLI::execute(engine, 1, "service unrecord names web.example").ok);
    REQUIRE(DeviceCLI::execute(engine, 1, "service unpage web /").ok);
    REQUIRE_FALSE(DeviceCLI::execute(engine, -1, "show services").ok);
}

TEST_CASE("Service configuration round trips and isolated runs discard runtime requests", "[services][save]") {
    auto topology = serviceTopology();
    topology.removeNode(2);
    const auto document = TopologyLoader::toJson(topology);
    REQUIRE(TopologyLoader::toJson(TopologyLoader::fromJson(document)) == document);
    SimulationEngine first(serviceTopology());
    first.networkServices().request(first, 0, 2, ServiceKind::Http, 80, "/");
    SimulationEngine second(first.getTopology());
    REQUIRE(second.networkServices().requests().empty());
    REQUIRE_FALSE(second.hasEvents());
    REQUIRE(second.getTopology().getNode(2)->getServices() == first.getTopology().getNode(2)->getServices());
    REQUIRE(DeviceCLI::execute(second, 2, "service stop web").ok);
    REQUIRE(first.getTopology().getNode(2)->getServices()[0].enabled);
}

TEST_CASE("Topology loading validates service schema without numeric truncation", "[services][save]") {
    auto document = TopologyLoader::toJson(serviceTopology());
    SECTION("fractional port") { document["nodes"][2]["services"][0]["port"] = 80.5; }
    SECTION("overflowing port") { document["nodes"][2]["services"][0]["port"] = 4294967376ULL; }
    SECTION("fractional status") { document["nodes"][2]["services"][0]["pages"]["/"]["status"] = 200.5; }
    SECTION("wrong collection type") { document["nodes"][2]["services"] = nlohmann::json::object(); }
    SECTION("ambiguous DNS names") { document["nodes"][2]["services"][1]["records"]["WEB.EXAMPLE."] = "192.0.2.22"; }
    REQUIRE_THROWS(TopologyLoader::fromJson(document));
}

TEST_CASE("Discovery preserves local services unless it supplies explicit configuration", "[services][discovery]") {
    auto topology = serviceTopology();
    auto snapshot = TopologyLoader::toJson(topology);
    snapshot["nodes"][2].erase("services");
    topology.synchronizeFrom(TopologyLoader::fromJson(snapshot));
    REQUIRE(topology.getNode(2)->getServices().size() == 2);
    snapshot["nodes"][2]["services"] = nlohmann::json::array();
    REQUIRE(topology.synchronizeFrom(TopologyLoader::fromJson(snapshot)));
    REQUIRE(topology.getNode(2)->getServices().empty());
}

TEST_CASE("Service request validation is bounded and has no side effects on failure", "[services]") {
    SimulationEngine engine(serviceTopology());
    REQUIRE_THROWS(engine.networkServices().request(engine, -1, 2, ServiceKind::Http, 80, "/"));
    REQUIRE_THROWS(engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 0, "/"));
    REQUIRE_THROWS(engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "bad path"));
    REQUIRE_THROWS(engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/", std::numeric_limits<double>::quiet_NaN()));
    REQUIRE(engine.networkServices().requests().empty());
    REQUIRE_FALSE(engine.hasEvents());
    for (int i = 0; i < 256; ++i) engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/");
    REQUIRE_THROWS(engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/"));
    engine.run();
    REQUIRE_NOTHROW(engine.networkServices().request(engine, 0, 2, ServiceKind::Http, 80, "/"));
    REQUIRE(engine.networkServices().requests().size() == 256);
}
