#include <catch2/catch_test_macros.hpp>
#include "network/Topology.hpp"
#include "network/Routing.hpp"
#include "engine/core/SimulationEngine.hpp"
#include <limits>

using kns::Topology;
using kns::Routing;
using kns::LinkMode;

TEST_CASE("Routing calculates shortest paths correctly in a chain topology", "[network][routing]")
{
    // Node chain: 0 <-> 1 <-> 2
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 10.0, 20.0, 0.0, LinkMode::FULL_DUPLEX);

    Routing routing;
    auto table = routing.buildRoutingTable(topo, 0);

    REQUIRE(table.size() == 3);

    // Source node (0 -> 0)
    REQUIRE(table[0].destination == 0);
    REQUIRE(table[0].next_hop == -1);
    REQUIRE(table[0].distance == 0.0);

    // Adjacent node (0 -> 1)
    REQUIRE(table[1].destination == 1);
    REQUIRE(table[1].next_hop == 1);
    REQUIRE(table[1].distance == 10.0);

    // Multi-hop node (0 -> 2)
    REQUIRE(table[2].destination == 2);
    REQUIRE(table[2].next_hop == 1); // should route via node 1
    REQUIRE(table[2].distance == 30.0);
}

TEST_CASE("Routing chooses faster indirect path over slower direct path", "[network][routing]")
{
    // Triangle: 0, 1, 2
    // Path 0-2: 20ms delay (direct)
    // Path 0-1-2: 5ms + 5ms = 10ms delay (indirect)
    Topology topo(3);
    topo.addLink(0, 2, 10.0, 20.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);

    Routing routing;
    auto table = routing.buildRoutingTable(topo, 0);

    REQUIRE(table[2].destination == 2);
    REQUIRE(table[2].next_hop == 1); // should route via 1, not 2 directly
    REQUIRE(table[2].distance == 10.0);
}

TEST_CASE("Routing handles unreachable nodes correctly", "[network][routing]")
{
    // 0 <-> 1  and an isolated node 2
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);

    Routing routing;
    auto table = routing.buildRoutingTable(topo, 0);

    REQUIRE(table[2].destination == 2);
    REQUIRE(table[2].next_hop == -1);
    REQUIRE(table[2].distance == std::numeric_limits<double>::infinity());
}

TEST_CASE("SimulationEngine getNextHop node index bounds validation", "[network][routing]")
{
    // 0 <-> 1  and isolated node 2
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);

    kns::SimulationEngine engine(topo);

    // Negative node indices should return -1
    REQUIRE(engine.getNextHop(-1, 1) == -1);
    REQUIRE(engine.getNextHop(0, -5) == -1);

    // Out of bounds node indices should return -1
    REQUIRE(engine.getNextHop(0, 99) == -1);
    REQUIRE(engine.getNextHop(99, 0) == -1);

    // Unreachable destination should return -1
    REQUIRE(engine.getNextHop(0, 2) == -1);

    // Valid next hop
    REQUIRE(engine.getNextHop(0, 1) == 1);
}

TEST_CASE("Routing and SimulationEngine ignore links that are DOWN", "[network][routing]")
{
    // Triangle: 0-1 (5ms), 0-2 (20ms), 1-2 (5ms)
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(0, 2, 10.0, 20.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);

    kns::SimulationEngine engine(topo);

    // Initial shortest path from 0 to 2 is via 1 (5ms + 5ms = 10ms < 20ms)
    REQUIRE(engine.getNextHop(0, 2) == 1);

    // Turn link 0-1 DOWN
    engine.toggleLinkUp(0, 1, false);

    // Now shortest path from 0 to 2 must reroute directly via 2 (20ms)
    REQUIRE(engine.getNextHop(0, 2) == 2);
    // Node 1 is rerouted via 2 (0 -> 2 -> 1, 25ms)
    REQUIRE(engine.getNextHop(0, 1) == 2);

    // Turn link 0-2 DOWN as well -> node 1 and 2 become unreachable from 0
    engine.toggleLinkUp(0, 2, false);
    REQUIRE(engine.getNextHop(0, 1) == -1);
    REQUIRE(engine.getNextHop(0, 2) == -1);

    // Turn links back UP
    engine.toggleLinkUp(0, 1, true);
    engine.toggleLinkUp(0, 2, true);
    REQUIRE(engine.getNextHop(0, 2) == 1);
}
