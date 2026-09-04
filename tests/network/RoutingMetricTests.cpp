#include <catch2/catch_test_macros.hpp>

#include "network/Topology.hpp"
#include "network/Routing.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "enums/LinkMode.hpp"
#include <limits>

using kns::Topology;
using kns::Routing;
using kns::RoutingMetric;
using kns::SimulationEngine;
using kns::LinkMode;

TEST_CASE("Routing metric: Delay minimizes total propagation delay", "[network][routing][metric]")
{
    // Triangle: 0-1-2 vs 0-2
    // Path A (indirect): 0-1 (5ms, 10Mbps), 1-2 (5ms, 10Mbps) -> total delay 10ms
    // Path B (direct): 0-2 (30ms, 100Mbps) -> total delay 30ms
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(0, 2, 100.0, 30.0, 0.0, LinkMode::FULL_DUPLEX);

    Routing routing;
    auto table = routing.buildRoutingTable(topo, 0, RoutingMetric::Delay);

    // Should choose indirect path 0 -> 1 -> 2 (10ms < 30ms)
    REQUIRE(table[2].destination == 2);
    REQUIRE(table[2].next_hop == 1);
    REQUIRE(table[2].distance == 10.0);
}

TEST_CASE("Routing metric: Bandwidth maximizes bottleneck capacity", "[network][routing][metric]")
{
    // Triangle: 0-1-2 vs 0-2
    // Path A (indirect): 0-1 (5ms, 10Mbps), 1-2 (5ms, 10Mbps) -> bottleneck 10Mbps
    // Path B (direct): 0-2 (30ms, 100Mbps) -> bottleneck 100Mbps
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(0, 2, 100.0, 30.0, 0.0, LinkMode::FULL_DUPLEX);

    Routing routing;
    auto table = routing.buildRoutingTable(topo, 0, RoutingMetric::Bandwidth);

    // Should choose direct path 0 -> 2 with 100Mbps capacity
    REQUIRE(table[2].destination == 2);
    REQUIRE(table[2].next_hop == 2);
    REQUIRE(table[2].distance == 100.0);
}

TEST_CASE("Routing metric: HopCount minimizes number of hops", "[network][routing][metric]")
{
    // Path A (indirect): 0-1 (1ms, 100Mbps), 1-2 (1ms, 100Mbps) -> 2 hops
    // Path B (direct): 0-2 (100ms, 1Mbps) -> 1 hop
    Topology topo(3);
    topo.addLink(0, 1, 100.0, 1.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 100.0, 1.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(0, 2, 1.0, 100.0, 0.0, LinkMode::FULL_DUPLEX);

    Routing routing;
    auto table = routing.buildRoutingTable(topo, 0, RoutingMetric::HopCount);

    // Should choose direct path 0 -> 2 (1 hop < 2 hops)
    REQUIRE(table[2].destination == 2);
    REQUIRE(table[2].next_hop == 2);
    REQUIRE(table[2].distance == 1.0);
}

TEST_CASE("Routing metric: DelayBandwidth favors high-bandwidth low-latency paths", "[network][routing][metric]")
{
    // Path A: 0-1 (10ms, 10Mbps -> 1.0) + 1-2 (10ms, 10Mbps -> 1.0) = cost 2.0
    // Path B: 0-2 (40ms, 100Mbps -> 0.4) = cost 0.4
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(0, 2, 100.0, 40.0, 0.0, LinkMode::FULL_DUPLEX);

    Routing routing;
    auto table = routing.buildRoutingTable(topo, 0, RoutingMetric::DelayBandwidth);

    // Direct path has lower cost (0.4 < 2.0)
    REQUIRE(table[2].destination == 2);
    REQUIRE(table[2].next_hop == 2);
    REQUIRE(table[2].distance == 0.4);
}

TEST_CASE("SimulationEngine dynamically switches routing metrics", "[network][routing][metric]")
{
    // Topology with competing delay vs bandwidth paths:
    // 0-1-2: lower delay (5+5=10ms), lower bandwidth (10Mbps)
    // 0-2: higher delay (30ms), higher bandwidth (100Mbps)
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(0, 2, 100.0, 30.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topo);

    // Default metric is Delay -> routes via 1
    REQUIRE(engine.getRoutingMetric() == RoutingMetric::Delay);
    REQUIRE(engine.getNextHop(0, 2) == 1);

    // Switch metric to Bandwidth -> immediately rebuilds routes to 2
    engine.setRoutingMetric(RoutingMetric::Bandwidth);
    REQUIRE(engine.getRoutingMetric() == RoutingMetric::Bandwidth);
    REQUIRE(engine.getNextHop(0, 2) == 2);

    // Switch metric back to Delay -> immediately rebuilds routes to 1
    engine.setRoutingMetric(RoutingMetric::Delay);
    REQUIRE(engine.getRoutingMetric() == RoutingMetric::Delay);
    REQUIRE(engine.getNextHop(0, 2) == 1);

    // Switch metric to HopCount -> direct path (1 hop)
    engine.setRoutingMetric(RoutingMetric::HopCount);
    REQUIRE(engine.getRoutingMetric() == RoutingMetric::HopCount);
    REQUIRE(engine.getNextHop(0, 2) == 2);
}

TEST_CASE("Routing metrics handle unreachable nodes consistently", "[network][routing][metric]")
{
    // 0 <-> 1 and isolated node 2
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);

    Routing routing;
    const double inf = std::numeric_limits<double>::infinity();

    SECTION("Delay metric") {
        auto table = routing.buildRoutingTable(topo, 0, RoutingMetric::Delay);
        REQUIRE(table[2].next_hop == -1);
        REQUIRE(table[2].distance == inf);
    }

    SECTION("Bandwidth metric") {
        auto table = routing.buildRoutingTable(topo, 0, RoutingMetric::Bandwidth);
        REQUIRE(table[2].next_hop == -1);
        REQUIRE(table[2].distance == 0.0);
    }

    SECTION("HopCount metric") {
        auto table = routing.buildRoutingTable(topo, 0, RoutingMetric::HopCount);
        REQUIRE(table[2].next_hop == -1);
        REQUIRE(table[2].distance == inf);
    }

    SECTION("DelayBandwidth metric") {
        auto table = routing.buildRoutingTable(topo, 0, RoutingMetric::DelayBandwidth);
        REQUIRE(table[2].next_hop == -1);
        REQUIRE(table[2].distance == inf);
    }
}

TEST_CASE("Routing metric calculation is strictly deterministic", "[network][routing][metric]")
{
    Topology topo(4);
    topo.addLink(0, 1, 50.0, 10.0, 0.0);
    topo.addLink(1, 2, 20.0, 15.0, 0.0);
    topo.addLink(0, 3, 10.0, 5.0, 0.0);
    topo.addLink(3, 2, 10.0, 5.0, 0.0);

    Routing routing;

    for (auto metric : {RoutingMetric::Delay, RoutingMetric::Bandwidth, RoutingMetric::HopCount, RoutingMetric::DelayBandwidth}) {
        auto table1 = routing.buildRoutingTable(topo, 0, metric);
        auto table2 = routing.buildRoutingTable(topo, 0, metric);

        REQUIRE(table1.size() == table2.size());
        for (std::size_t i = 0; i < table1.size(); ++i) {
            REQUIRE(table1[i].destination == table2[i].destination);
            REQUIRE(table1[i].next_hop == table2[i].next_hop);
            REQUIRE(table1[i].distance == table2[i].distance);
        }
    }
}
