#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include "engine/core/SimulationEngine.hpp"

using namespace kns;

TEST_CASE("ACKs resume a workload larger than the send window exactly once", "[tcp][generation][window]")
{
    Topology topology(2);
    topology.addLink(0, 1, 100.0, 1.0);
    SimulationEngine engine(topology);
    engine.setGlobalPacketSize(100);
    auto& session = engine.createTCPSession(0, 1);
    auto& client = session.getClientConnection();
    auto& server = session.getServerConnection();
    client = TCPConnection(TCPState::ESTABLISHED, 1000, 500, 0, 1);
    server = TCPConnection(TCPState::ESTABLISHED, 500, 1000, 1, 0);
    client.setSendWindow(100);
    engine.generatePackets(0.0, session);
    engine.generatePackets(0.0, session);
    REQUIRE(session.hasPendingGeneration());
    REQUIRE(engine.processEvent());
    REQUIRE(client.getSendBufferSize() == 1);
    REQUIRE_FALSE(session.hasPendingGeneration());
    engine.generatePackets(engine.now(), session);
    REQUIRE_FALSE(session.hasPendingGeneration());
    REQUIRE(engine.peekNextEventTime() > engine.now());

    int events = 0;
    while (!session.isCloseRequest() && events++ < 500) {
        REQUIRE(engine.processEvent());
        REQUIRE(client.getSendNext() - client.getSendUnacknowledged() <= 100);
    }
    REQUIRE(session.isCloseRequest());
    REQUIRE(session.isComplete());
    REQUIRE(session.isDataAcknowledged());
    REQUIRE_FALSE(session.hasPendingGeneration());
    REQUIRE(server.getExpectedAckNum() - 1000 ==
        static_cast<std::uint32_t>(engine.getPacketsPerRoute()) * 100U);
    REQUIRE(engine.getStats().data_packets_delivered == engine.getPacketsPerRoute());
    engine.run();
    REQUIRE(session.getState() == TCPState::CLOSED);
}

TEST_CASE("Oversized payload fails before starting generation and can be retried", "[tcp][generation][window]")
{
    SimulationEngine engine(Topology(2));
    auto& session = engine.createTCPSession(0, 1);
    session.getClientConnection() = TCPConnection(TCPState::ESTABLISHED, 1000, 500, 0, 1);
    session.getServerConnection() = TCPConnection(TCPState::ESTABLISHED, 500, 1000, 1, 0);
    engine.setGlobalPacketSize(101);
    session.getClientConnection().setSendWindow(100);
    REQUIRE_THROWS_AS(engine.generatePackets(0.0, session), std::invalid_argument);
    REQUIRE_FALSE(session.hasGeneratedTraffic());
    REQUIRE_FALSE(session.hasPendingGeneration());
    REQUIRE_FALSE(engine.hasEvents());
    session.getClientConnection().setSendWindow(101);
    REQUIRE_NOTHROW(engine.generatePackets(0.0, session));
    REQUIRE(session.hasGeneratedTraffic());
    REQUIRE(session.hasPendingGeneration());
}
