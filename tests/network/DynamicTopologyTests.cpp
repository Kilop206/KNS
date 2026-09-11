#include <catch2/catch_test_macros.hpp>

#include "network/Topology.hpp"
#include "network/Link.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"
#include "network/utils/PacketUtils.hpp"
#include "enums/LinkMode.hpp"
#include "enums/TCPState.hpp"

using namespace kns;

TEST_CASE("Dynamic topology: packet in transit reaches destination after link removal", "[network][topology][dynamic]")
{
    // Test A & D: packet already accepted on link continues to destination even if link is deleted
    Topology topo(2);
    topo.addLink(0, 1, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topo);
    auto& session = engine.createTCPSession(0, 1);

    Packet pkt(0, 1, 0, engine.now(), 1000, session.getSession_id());
    pkt.packet_type = PacketType::DATA;

    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, pkt));
    REQUIRE(engine.getPacketsInTransit().size() == 1);

    // Remove the link while packet is mid-flight
    REQUIRE(engine.deleteLink(0, 1));
    REQUIRE(engine.getTopology().getLinks().empty());

    // Execute arrival
    engine.run();

    // Packet must have been delivered, packets_in_transit must be clean
    REQUIRE(engine.getStats().packets_delivered == 1);
    REQUIRE(engine.getPacketsInTransit().empty());
}

TEST_CASE("Dynamic topology: Link DOWN prevents new transmissions and UP restores them", "[network][topology][dynamic]")
{
    // Test B & C: link UP -> accepted, DOWN -> rejected, UP -> accepted
    Topology topo(2);
    topo.addLink(0, 1, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topo);
    auto& session = engine.createTCPSession(0, 1);

    // 1. Send packet while UP
    Packet p1(0, 1, 0, engine.now(), 1000, session.getSession_id());
    p1.packet_type = PacketType::DATA;
    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, p1));

    // 2. Set link DOWN
    REQUIRE(engine.toggleLinkUp(0, 1, false));

    // New transmission must be rejected
    Packet p2(0, 1, 0, engine.now(), 1000, session.getSession_id());
    p2.packet_type = PacketType::DATA;
    REQUIRE_FALSE(PacketUtils::sendPacketThroughTopology(engine, p2));

    // 3. Set link back UP
    REQUIRE(engine.toggleLinkUp(0, 1, true));

    // Transmission is accepted again
    Packet p3(0, 1, 0, engine.now(), 1000, session.getSession_id());
    p3.packet_type = PacketType::DATA;
    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, p3));

    // Run until finish
    engine.run();

    // The topology test injects DATA packets without establishing a TCP connection.
    // Therefore no TCP ACK is generated; this test verifies only transmission
    // acceptance/rejection across the Link DOWN/UP transitions.
    REQUIRE(engine.getStats().packets_sent == 2);
    REQUIRE(engine.getStats().packets_delivered == 2);
    REQUIRE(engine.getPacketsInTransit().empty());
}

TEST_CASE("Dynamic topology: intermediate link removal causes packet loss without crash", "[network][topology][dynamic]")
{
    // Chain: 0 <-> 1 <-> 2
    Topology topo(3);
    topo.addLink(0, 1, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);
    topo.addLink(1, 2, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topo);
    auto& session = engine.createTCPSession(0, 2);

    Packet pkt(0, 2, 0, engine.now(), 1000, session.getSession_id());
    pkt.packet_type = PacketType::DATA;

    // Send from 0 towards 2 (first hop 0 -> 1)
    REQUIRE(PacketUtils::sendPacketThroughTopology(engine, pkt));
    REQUIRE(engine.getPacketsInTransit().size() == 1);

    // While in flight from 0 to 1, link 1-2 is removed
    REQUIRE(engine.deleteLink(1, 2));

    // Run simulation
    engine.run();

    // Hop 0->1 arrived, but forward to 2 failed because link 1-2 was removed
    REQUIRE(engine.getStats().packets_lost >= 1);
    REQUIRE(engine.getPacketsInTransit().empty());
}

TEST_CASE("Dynamic topology: topology mutation during TCP activity executes safely and deterministically", "[network][topology][dynamic]")
{
    // Test E: TCP activity with link failure
    Topology topo(2);
    topo.addLink(0, 1, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topo);
    engine.startTCPConnection(0, 1);

    // Schedule a link failure right at t=0.005 (before SYN arrives at t=0.0108)
    engine.scheduleLinkFailure(0.005, 0, 1, false);

    // Run simulation
    REQUIRE_NOTHROW(engine.run());

    // Engine finishes without crashing; session exists
    REQUIRE(engine.getTCPSessions().size() == 1);
    REQUIRE(engine.getPacketsInTransit().empty());
}

TEST_CASE(
    "Dynamic topology: packet arriving at removed node is handled safely",
    "[network][topology][dynamic]"
)
{
    // The packet is accepted before the destination node is removed.
    // Its arrival event must remain safe after the topology mutation.
    Topology topo(2);
    topo.addLink(
        0,
        1,
        10.0,
        10.0,
        0.0,
        LinkMode::FULL_DUPLEX
    );

    SimulationEngine engine(topo);
    auto& session = engine.createTCPSession(0, 1);

    Packet pkt(
        0,
        1,
        0,
        engine.now(),
        1000,
        session.getSession_id()
    );
    pkt.packet_type = PacketType::DATA;

    REQUIRE(
        PacketUtils::sendPacketThroughTopology(engine, pkt)
    );
    REQUIRE(engine.getPacketsInTransit().size() == 1);

    // Remove the destination node while the packet is still in flight.
    REQUIRE(engine.deleteNode(1));

    // The scheduled PacketReceivedEvent must execute safely.
    REQUIRE_NOTHROW(engine.run());

    // The in-flight packet must not remain stuck after its arrival event.
    REQUIRE(engine.getPacketsInTransit().empty());

    // The removed node remains as an inactive slot rather than being reused
    // or physically erased, preserving node indices.
    const Node* removed_node = engine.getTopology().getNode(1);
    REQUIRE(removed_node != nullptr);
    REQUIRE_FALSE(removed_node->isActive());
}

TEST_CASE(
    "Dynamic topology: removed nodes cannot be reconnected implicitly",
    "[network][topology][dynamic]"
)
{
    Topology initial_topology(3);
    SimulationEngine engine(initial_topology);
    engine.createLink(0, 1, 10.0, 5.0);
    auto& topology = engine.getTopology();

    REQUIRE(engine.deleteNode(1));
    REQUIRE(topology.getLinks().empty());
    REQUIRE_THROWS_AS(
        topology.addLink(0, 1, 10.0, 5.0),
        std::invalid_argument
    );

    REQUIRE(topology.getLinks().empty());
    REQUIRE(engine.getNextHop(0, 1) == -1);
}

TEST_CASE(
    "Dynamic topology: link-id events control parallel links independently",
    "[network][topology][dynamic]"
)
{
    Topology topo(2);
    auto first = topo.addLinkPtr(0, 1, 10.0, 5.0);
    auto second = topo.addLinkPtr(0, 1, 20.0, 5.0);

    SimulationEngine engine(topo);

    // Endpoint-based mutations reject this ambiguous pair.
    REQUIRE_FALSE(engine.toggleLinkUp(0, 1, false));
    REQUIRE(first->isUp());
    REQUIRE(second->isUp());

    engine.scheduleLinkFailure(1.0, first->getId(), false);
    engine.scheduleLinkFailure(2.0, second->getId(), false);
    engine.scheduleLinkFailure(3.0, first->getId(), true);

    REQUIRE(engine.processEvent());
    REQUIRE_FALSE(first->isUp());
    REQUIRE(second->isUp());
    REQUIRE(engine.getNextHop(0, 1) == 1);

    REQUIRE(engine.processEvent());
    REQUIRE_FALSE(first->isUp());
    REQUIRE_FALSE(second->isUp());
    REQUIRE(engine.getNextHop(0, 1) == -1);

    REQUIRE(engine.processEvent());
    REQUIRE(first->isUp());
    REQUIRE_FALSE(second->isUp());
    REQUIRE(engine.getNextHop(0, 1) == 1);

    REQUIRE(engine.deleteLinkById(first->getId()));
    REQUIRE(engine.getTopology().getLinks().size() == 1);
    REQUIRE(engine.getTopology().getLinks().front()->getId() == second->getId());
}
