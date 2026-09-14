#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/PacketGenerationEvent.hpp"
#include "engine/events/PacketReceivedEvent.hpp"

using namespace kns;

TEST_CASE("DATA acknowledgement is distinct from workload generation", "[tcp][close]")
{
    TCPSession session;
    REQUIRE_FALSE(session.isDataAcknowledged());
    session.markTrafficGenerated();
    session.setTotalPackets(1);
    REQUIRE_FALSE(session.isDataAcknowledged());
    session.incrementPacketsSent();
    REQUIRE(session.isDataAcknowledged());
    auto& server = session.getServerConnection();
    server = TCPConnection(TCPState::ESTABLISHED, 500, 0, 1, 0);
    TCPSegment segment;
    segment.seq = 500;
    segment.payload = {1};
    REQUIRE(server.queueSentSegment(segment, 0.0));
    REQUIRE(session.isComplete());
    REQUIRE_FALSE(session.isDataAcknowledged());
    REQUIRE(server.receive_ack(501, 0.1));
    REQUIRE(session.isDataAcknowledged());
}

TEST_CASE("Automatic FIN waits for lost tail DATA to be recovered and acknowledged", "[tcp][close][loss]")
{
    Topology topology(2);
    auto link = topology.addLinkPtr(0, 1, 100.0, 1.0);
    SimulationEngine engine(topology);
    engine.setGlobalPacketSize(100);
    auto& session = engine.createTCPSession(0, 1);
    const auto id = session.getSession_id();
    auto& client = session.getClientConnection();
    auto& server = session.getServerConnection();
    client = TCPConnection(TCPState::ESTABLISHED, 1000, 500, 0, 1);
    server = TCPConnection(TCPState::ESTABLISHED, 500, 1000, 1, 0);
    session.markTrafficGenerated();
    session.setTotalPackets(2);
    engine.schedule(std::make_unique<PacketGenerationEvent>(0.0, 0, 1, id));
    REQUIRE(engine.processEvent());
    link->setLossProb(1.0);
    REQUIRE(engine.processEvent());
    REQUIRE(session.isComplete());
    REQUIRE_FALSE(session.isDataAcknowledged());
    REQUIRE(client.getSendBufferSize() == 2);
    REQUIRE(engine.getStats().packets_lost == 1);
    link->setLossProb(0.0);

    for (int i = 0; i < 10 && client.getSendBufferSize() == 2; ++i) {
        REQUIRE(engine.processEvent());
    }
    REQUIRE(client.getSendBufferSize() == 1);
    REQUIRE(server.getExpectedAckNum() == 1100);
    REQUIRE_FALSE(session.isCloseRequest());
    REQUIRE(client.getTcpState() == TCPState::ESTABLISHED);

    Packet duplicate_ack(1, 0, 0, engine.now(), 40, id);
    duplicate_ack.tcp.flags = TCPFlag::ACK;
    duplicate_ack.tcp.ack = 1100;
    engine.schedule(std::make_unique<PacketReceivedEvent>(engine.now(), duplicate_ack));
    REQUIRE(engine.processEvent());
    REQUIRE_FALSE(session.isCloseRequest());

    for (int i = 0; i < 100 && !session.isCloseRequest(); ++i) {
        REQUIRE(engine.processEvent());
    }
    REQUIRE(session.isCloseRequest());
    REQUIRE(session.isDataAcknowledged());
    REQUIRE(server.getExpectedAckNum() - 1000 == 200);
    REQUIRE(client.getSendUnacknowledged() == 1200);
    REQUIRE(client.getTcpState() == TCPState::ESTABLISHED);
    engine.run();
    REQUIRE(session.getState() == TCPState::CLOSED);
}
