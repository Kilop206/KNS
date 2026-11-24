#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"

TEST_CASE("Transmission observers identify the accepted cable while preserving source-hop metadata",
          "[network][packet-observer]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 100, 1);
    auto chosen = topology.addLinkPtr(0, 1, 20, 2);
    kns::SimulationEngine engine(topology);
    const auto live = engine.getTopology().getLinks()[1];
    kns::Packet packet(0, 1, 0, 0, 100, 12);
    packet.link_id = chosen->getId() + 1000; // Identity carried over from an earlier hop.
    packet.hop_count = 3;
    int observed = 0;
    engine.setPacketObserver([&](const kns::Packet& p, auto session, int from, int to,
                                 double departure, double arrival) {
        ++observed;
        REQUIRE(p.link_id == chosen->getId());
        REQUIRE(p.current_node == packet.current_node);
        REQUIRE(p.hop_count == packet.hop_count);
        REQUIRE(session == 12);
        REQUIRE(from == 0);
        REQUIRE(to == 1);
        REQUIRE(arrival > departure);
    });
    REQUIRE(engine.sendPacket(packet, *live, 0));
    REQUIRE(observed == 1);
    REQUIRE(packet.link_id == chosen->getId() + 1000);
    REQUIRE(engine.getPacketsInTransit().front().link_id == chosen->getId());
}
