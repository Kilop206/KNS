#include <catch2/catch_test_macros.hpp>

#include "engine/core/SimulationEngine.hpp"
#include "engine/events/PrintEvent.hpp"
#include "engine/events/TCPTimeoutEvent.hpp"
#include "network/utils/PacketUtils.hpp"

using namespace kns;

TEST_CASE("Validation accepts TCP data recovered after network loss", "[core][validation][tcp][rto]")
{
    Topology topology(2);
    auto link = topology.addLinkPtr(0, 1, 10.0, 1.0, 1.0);
    SimulationEngine engine(topology);
    auto& session = engine.createTCPSession(0, 1);
    auto& client = session.getClientConnection();
    client = TCPConnection(TCPState::ESTABLISHED, 100, 500, 0, 1);
    session.getServerConnection() = TCPConnection(TCPState::ESTABLISHED, 500, 100, 1, 0);
    session.markTrafficGenerated();
    session.setTotalPackets(1);

    Packet data(0, 1, 0, 0.0, 100, session.getSession_id());
    data.packet_type = PacketType::DATA;
    data.tcp.seq = 100;
    data.tcp.ack = 500;
    data.tcp.flags = TCPFlag::ACK | TCPFlag::PSH;
    data.tcp.payload.assign(100, 0x41);
    REQUIRE(client.queueSentSegment(data.tcp, 0.0));
    session.incrementPacketsSent();
    REQUIRE_FALSE(PacketUtils::sendPacketThroughTopology(engine, data));
    REQUIRE(engine.getStats().packets_lost == 1);
    link->setLossProb(0.0);
    engine.schedule(std::make_unique<TCPTimeoutEvent>(client.getCurrentRTO(), session.getSession_id(), 100));
    REQUIRE_FALSE(engine.validateSimulation().passed());
    engine.run();
    REQUIRE(session.getState() == TCPState::CLOSED);
    REQUIRE(client.getSendBufferSize() == 0);
    REQUIRE(engine.getStats().packets_lost == 1);
    REQUIRE(engine.validateSimulation().passed());
    REQUIRE_FALSE(engine.validateSimulation().loss_free);
}

TEST_CASE("Validation rejects incomplete or inconsistent simulations", "[core][validation]")
{
    Topology topology(2);
    auto link = topology.addLinkPtr(0, 1, 10.0, 1.0);
    SimulationEngine engine(topology);
    engine.startTCPConnection(0, 1);
    engine.run();
    const auto baseline = engine.validateSimulation();
    INFO("sessions=" << baseline.completed_sessions << "/" << baseline.total_sessions
         << " sent=" << baseline.packets_sent << " delivered=" << baseline.packets_delivered
         << " lost=" << baseline.packets_lost << " events=" << engine.hasEvents()
         << " transit=" << engine.getPacketsInTransit().size()
         << " queue=" << link->getQueueSize());
    const auto& baseline_session = engine.getTCPSession(0);
    INFO("client=" << static_cast<int>(baseline_session.getClientConnection().getTcpState())
         << " server=" << static_cast<int>(baseline_session.getServerConnection().getTcpState())
         << " generated=" << baseline_session.hasGeneratedTraffic()
         << " complete=" << baseline_session.isComplete()
         << " client-send=" << baseline_session.getClientConnection().getSendBufferSize()
         << " server-send=" << baseline_session.getServerConnection().getSendBufferSize()
         << " client-receive=" << baseline_session.getClientConnection().getReceiveBufferSize()
         << " server-receive=" << baseline_session.getServerConnection().getReceiveBufferSize());
    REQUIRE(baseline.passed());

    SECTION("Unfinished session") {
        engine.getTCPSession(0).getClientConnection() = TCPConnection(TCPState::ESTABLISHED, 0, 0, 0, 1);
    }
    SECTION("Unaccounted packet") {
        ++engine.getStats().packets_sent;
    }
    SECTION("Impossible delivery count") {
        engine.getStats().packets_delivered = engine.getStats().packets_sent + 1;
    }
    SECTION("Negative counters") {
        engine.getStats().packets_lost = -1;
    }
    SECTION("Orphaned queue entry") {
        link->enqueueTransmission(0, 1, engine.now(), engine.now() + 1.0);
    }
    SECTION("Packet and event still in transit") {
        Packet packet(0, 1, 0, engine.now(), 100, 999);
        REQUIRE(engine.sendPacket(packet, *link, engine.now()));
    }
    REQUIRE_FALSE(engine.validateSimulation().passed());
}
