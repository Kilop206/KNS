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

    // Each DATA packet generates an ACK back once the link is UP, so 2 DATA + 2 ACK = 4 packets delivered
    REQUIRE(engine.getStats().packets_sent == 4);
    REQUIRE(engine.getStats().packets_delivered == 4);
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
