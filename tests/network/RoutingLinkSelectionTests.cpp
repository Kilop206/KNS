#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "network/utils/PacketUtils.hpp"

using namespace kns;

TEST_CASE("Forwarding uses the Dijkstra first edge for every metric and insertion order", "[network][routing][parallel]")
{
    for (const auto metric : {RoutingMetric::Delay, RoutingMetric::Bandwidth,
            RoutingMetric::HopCount, RoutingMetric::DelayBandwidth}) {
        for (const bool best_first : {false, true}) {
            CAPTURE(static_cast<int>(metric), best_first);
            Topology topology(3);
            Topology::LinkPtr best, other;
            if (best_first) best = topology.addLinkPtr(0, 1, 100.0, 10.0);
            other = topology.addLinkPtr(0, 1, 10.0, 20.0);
            if (!best_first) best = topology.addLinkPtr(0, 1, 100.0, 10.0);
            topology.addLink(1, 2, 1000.0, 1.0);
            SimulationEngine engine(topology);
            engine.setRoutingMetric(metric);
            const auto chosen = metric == RoutingMetric::HopCount && !best_first ? other : best;
            REQUIRE(engine.getRoutingTable(0)[2].link_id == chosen->getId());
            REQUIRE_FALSE(engine.getRoutingTable(0)[0].link_id.has_value());
            Packet packet(0, 2, 0, 0.0, 100, 999);
            REQUIRE(PacketUtils::sendPacketThroughTopology(engine, packet));
            REQUIRE(engine.getPacketsInTransit().front().link_id == chosen->getId());
            engine.run();
            chosen->setUp(false);
            const auto fallback = chosen == best ? other : best;
            REQUIRE(PacketUtils::sendPacketThroughTopology(engine, packet));
            REQUIRE(engine.getPacketsInTransit().front().link_id == fallback->getId());
            engine.run();
        }
    }
}

TEST_CASE("Routing edge selection excludes reverse SIMPLEX links", "[network][routing][parallel]")
{
    for (const auto metric : {RoutingMetric::Delay, RoutingMetric::Bandwidth,
            RoutingMetric::HopCount, RoutingMetric::DelayBandwidth}) {
        Topology topology(3);
        topology.addLink(1, 0, 1000.0, 0.0, 0.0, LinkMode::SIMPLEX);
        auto forward = topology.addLinkPtr(0, 1, 10.0, 10.0, 0.0, LinkMode::SIMPLEX);
        SimulationEngine engine(topology);
        engine.setRoutingMetric(metric);
        REQUIRE_FALSE(engine.getRoutingTable(0)[2].link_id.has_value());
        Packet packet(0, 1, 0, 0.0, 100, 999);
        REQUIRE(PacketUtils::sendPacketThroughTopology(engine, packet));
        REQUIRE(engine.getPacketsInTransit().front().link_id == forward->getId());
    }
}
