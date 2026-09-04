#include <catch2/catch_test_macros.hpp>
#include "network/Topology.hpp"
#include "network/Routing.hpp"
#include "network/utils/PacketUtils.hpp"
#include "engine/core/SimulationEngine.hpp"
#include <limits>

using kns::Topology;
using kns::Routing;
using kns::LinkMode;
using kns::PacketUtils;
using kns::Packet;
using kns::PacketType;
using kns::SimulationEngine;
using kns::Link;

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

    // Same node (current == destination) has no next hop
    REQUIRE(engine.getNextHop(0, 0) == -1);
    REQUIRE(engine.getNextHop(1, 1) == -1);

    // Valid next hop
    REQUIRE(engine.getNextHop(0, 1) == 1);
    REQUIRE(engine.getNextHop(1, 0) == 0);

    // Empty topology safely returns -1
    Topology empty_topo(0);
    kns::SimulationEngine empty_engine(empty_topo);
    REQUIRE(empty_engine.getNextHop(0, 0) == -1);
    REQUIRE(empty_engine.getNextHop(-1, 0) == -1);

    // Dynamic topology mutation updates bounds correctly
    const int new_node = engine.createNode(); // creates node 3
    REQUIRE(engine.getNextHop(0, new_node) == -1); // unreachable
    engine.createLink(1, new_node, 10.0, 5.0);
    REQUIRE(engine.getNextHop(0, new_node) == 1); // now reachable via 1
    engine.deleteNode(new_node);
    REQUIRE(engine.getNextHop(0, new_node) == -1);
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

TEST_CASE("Link UP and DOWN integration with packet transmission and alternate paths", "[network][routing][link]")
{
    // Triangle: 0-1 (5ms), 1-2 (5ms), 0-2 (50ms alternate path)
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(0, 2, 10.0, 50.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topo);
    auto& session = engine.createTCPSession(0, 2);

    // 1. Initial state: UP -> shortest route 0->2 goes via 1
    REQUIRE(engine.getNextHop(0, 2) == 1);

    Packet p1(0, 2, 0, engine.now(), 1000, session.getSession_id());
    p1.packet_type = PacketType::DATA;
    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, p1));

    // 2. Link 0-1 goes DOWN -> route avoids 0-1 and chooses alternate path 0-2
    REQUIRE(engine.toggleLinkUp(0, 1, false));
    REQUIRE(engine.getNextHop(0, 2) == 2);

    // Direct transmission on 0-1 must be rejected
    auto& links_from_0 = engine.getTopology().getLinksFromNode(0);
    Link* link_0_1 = nullptr;
    for (const auto& l : links_from_0) {
        if (l->getOtherNode(0) == 1) link_0_1 = l.get();
    }
    REQUIRE(link_0_1 != nullptr);
    REQUIRE_FALSE(link_0_1->isUp());
    REQUIRE_FALSE(engine.sendPacket(p1, *link_0_1, engine.now()));

    // But transmission towards node 2 succeeds using the alternate path 0-2
    Packet p2(0, 2, 0, engine.now(), 1000, session.getSession_id());
    p2.packet_type = PacketType::DATA;
    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, p2));

    // 3. Link 0-2 also goes DOWN -> node 2 is unreachable, transmission rejected
    REQUIRE(engine.toggleLinkUp(0, 2, false));
    REQUIRE(engine.getNextHop(0, 2) == -1);

    Packet p3(0, 2, 0, engine.now(), 1000, session.getSession_id());
    p3.packet_type = PacketType::DATA;
    REQUIRE_FALSE(PacketUtils::sendPacketThroughTopology(engine, p3));

    // 4. Link 0-1 returns UP -> route 0-2 is restored via 1 (0 -> 1 -> 2)
    REQUIRE(engine.toggleLinkUp(0, 1, true));
    REQUIRE(engine.getNextHop(0, 2) == 1);

    Packet p4(0, 2, 0, engine.now(), 1000, session.getSession_id());
    p4.packet_type = PacketType::DATA;
    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, p4));

    // Run engine and verify deliveries
    engine.run();
    REQUIRE(engine.getStats().packets_delivered > 0);
    REQUIRE(engine.getPacketsInTransit().empty());
}

TEST_CASE("Link UP and DOWN with parallel links selects available UP link", "[network][routing][link]")
{
    Topology topo(2);
    // Add two parallel links between 0 and 1
    auto l1 = topo.addLinkPtr(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    auto l2 = topo.addLinkPtr(0, 1, 20.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topo);
    auto& session = engine.createTCPSession(0, 1);

    // Turn link 1 DOWN, link 2 remains UP
    l1->setUp(false);
    REQUIRE_FALSE(l1->isUp());
    REQUIRE(l2->isUp());

    // sendPacketThroughTopology should choose the UP link
    Packet p(0, 1, 0, engine.now(), 1000, session.getSession_id());
    p.packet_type = PacketType::DATA;
    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, p));

    // Turn link 2 DOWN as well -> both DOWN
    l2->setUp(false);
    REQUIRE_FALSE(PacketUtils::sendPacketThroughTopology(engine, p));

    // Restore link 1 UP -> transmission succeeds again
    l1->setUp(true);
    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, p));
}
