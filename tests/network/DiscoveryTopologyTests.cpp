#include <catch2/catch_test_macros.hpp>

#include "engine/core/SimulationEngine.hpp"
#include "analysis/NetworkAnalyzer.hpp"
#include "network/TopologyLoader.hpp"
#include "network/utils/PacketUtils.hpp"

using namespace kns;

namespace {
nlohmann::json discoverySnapshot()
{
    return nlohmann::json::parse(R"({
        "schema_version":"1.0","name":"Discovered LAN",
        "nodes":[
            {"id":0,"external_id":"host:a","label":"Laptop","type":"computer","addresses":["192.0.2.10"]},
            {"id":1,"external_id":"segment:a","type":"network_segment"},
            {"id":2,"external_id":"device:b","label":"Gateway","type":"router","evidence":"default_route"}
        ],
        "links":[
            {"from":0,"to":1,"bandwidth":100,"delay":1,"loss":0,"inferred":true,"evidence":"shared_segment"},
            {"from":1,"to":2,"bandwidth":100,"delay":1,"loss":0,"inferred":true,"evidence":"shared_segment"}
        ]
    })");
}
}

TEST_CASE("Device topology JSON round trips metadata and tombstones", "[network][discovery]")
{
    auto topology = TopologyLoader::fromJson(discoverySnapshot());
    REQUIRE(topology.getNode(0)->getDeviceInfo().type == DeviceType::Computer);
    REQUIRE(topology.getNode(0)->getDeviceInfo().addresses == std::vector<std::string>{"192.0.2.10"});
    REQUIRE(topology.getLinks().front()->isInferred());
    topology.removeNode(2);
    const auto document = TopologyLoader::toJson(topology);
    REQUIRE(TopologyLoader::toJson(TopologyLoader::fromJson(document)) == document);
    REQUIRE(topology.cloneForRun().getNode(0)->getDeviceInfo() == topology.getNode(0)->getDeviceInfo());
}

TEST_CASE("Typed topology rejects invalid identities before replacing live state", "[network][discovery]")
{
    const auto original = discoverySnapshot();
    for (const auto& mutation : {"duplicate_id", "duplicate_identity", "missing_endpoint", "unknown_type", "fractional_id", "version"}) {
        CAPTURE(mutation);
        auto document = original;
        const std::string change = mutation;
        if (change == "duplicate_id") document["nodes"][1]["id"] = 0;
        if (change == "duplicate_identity") document["nodes"][1]["external_id"] = "host:a";
        if (change == "missing_endpoint") document["links"][0]["to"] = 9;
        if (change == "unknown_type") document["nodes"][0]["type"] = "magic";
        if (change == "fractional_id") document["nodes"][0]["id"] = 0.5;
        if (change == "version") document["schema_version"] = "9.0";
        REQUIRE_THROWS(TopologyLoader::fromJson(document));
    }
}

TEST_CASE("Live discovery preserves active identities and unchanged link queues", "[network][discovery][dynamic]")
{
    const auto snapshot = TopologyLoader::fromJson(discoverySnapshot());
    SimulationEngine engine(snapshot);
    auto& topology = engine.getTopology();
    const auto link = topology.getLinks().front();
    link->enqueueTransmission(0, 1, 0.0, 1.0);
    REQUIRE_FALSE(engine.synchronizeTopology(snapshot));
    REQUIRE(topology.getLinks().front() == link);
    REQUIRE(link->getQueueSize() == 1);

    auto document = discoverySnapshot();
    // Snapshot numbers changed; simulator numbers must not change.
    document["nodes"][0]["id"] = 2;
    document["nodes"][2]["id"] = 0;
    document["nodes"][0]["label"] = "Renamed laptop";
    document["links"][0]["from"] = 2;
    document["links"][1]["to"] = 0;
    REQUIRE(engine.synchronizeTopology(TopologyLoader::fromJson(document)));
    REQUIRE(topology.getNode(0)->getLabel() == "Renamed laptop");
    REQUIRE(topology.getNode(2)->getDeviceInfo().type == DeviceType::Router);
    REQUIRE(topology.getLinks().front() == link);
    REQUIRE(engine.getNextHop(0, 2) == 1);
}

TEST_CASE("Removed discovery devices never recycle in-flight numeric identities", "[network][discovery][dynamic]")
{
    SimulationEngine engine(TopologyLoader::fromJson(discoverySnapshot()));
    // The gateway may initiate diagnostics; it cannot act as a TCP listener.
    auto& session = engine.createTCPSession(2, 0);
    Packet packet(0, 2, 0, engine.now(), 1000, session.getSession_id());
    packet.packet_type = PacketType::DATA;
    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, packet));
    auto document = discoverySnapshot();
    document["nodes"].erase(2);
    document["links"].erase(1);
    REQUIRE(engine.synchronizeTopology(TopologyLoader::fromJson(document)));
    REQUIRE_FALSE(engine.getTopology().getNode(2)->isActive());
    REQUIRE(engine.getNextHop(0, 2) == -1);
    REQUIRE_NOTHROW(engine.run());
    REQUIRE(engine.getPacketsInTransit().empty());
    REQUIRE(engine.synchronizeTopology(TopologyLoader::fromJson(discoverySnapshot())));
    REQUIRE(engine.getTopology().size() == 4);
    REQUIRE_FALSE(engine.getTopology().getNode(2)->isActive());
    REQUIRE(engine.getTopology().getNode(3)->getDeviceInfo().external_id == "device:b");
    REQUIRE(engine.getNextHop(0, 3) == 1);
    const auto analysis = analysis::NetworkAnalyzer{}.analyze(engine.getTopology());
    REQUIRE(analysis.node_count == 3);
    REQUIRE(analysis.connected_components == 1);
    REQUIRE(analysis.unreachable_route_count == 0);
    REQUIRE(analysis.nodes.back().node_id == 3);
}
