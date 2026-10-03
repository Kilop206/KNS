#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/PacketReceivedEvent.hpp"

using namespace kns;

TEST_CASE("Valid zero window and reopen ACKs suspend and resume generation", "[tcp][peer-window]")
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
    client.setReceiveWindow(222);
    Packet ack(1, 0, 0, 0.0, 40, session.getSession_id());
    ack.tcp = server.buildAck();
    ack.tcp.window = 0;
    PacketReceivedEvent(0.0, ack).execute(engine);
    REQUIRE(client.getPeerWindow() == 0);
    REQUIRE_FALSE(session.hasPendingGeneration());
    REQUIRE_FALSE(client.canSend(1));
    ack.tcp.ack = 1001;
    ack.tcp.window = 100;
    PacketReceivedEvent(0.0, ack).execute(engine);
    REQUIRE(client.getPeerWindow() == 0);
    ack.tcp.ack = 1000;
    ack.tcp.seq = 499;
    PacketReceivedEvent(0.0, ack).execute(engine);
    REQUIRE(client.getPeerWindow() == 0);
    ack.tcp.seq = 500;
    PacketReceivedEvent(0.0, ack).execute(engine);
    REQUIRE(client.getPeerWindow() == 100);
    REQUIRE(session.hasPendingGeneration());
    bool observed = false;
    engine.setPacketObserver([&](const Packet& packet, auto, auto, auto, auto, auto) {
        if (!packet.tcp.payload.empty()) {
            REQUIRE(packet.tcp.window == 222);
            observed = true;
        }
    });
    for (int i = 0; i < 4 && !observed; ++i) REQUIRE(engine.processEvent());
    REQUIRE(observed);
    REQUIRE(client.getSendNext() - client.getSendUnacknowledged() == 100);
    REQUIRE_FALSE(client.canSend(1));
}

TEST_CASE("Handshake transfers peer receive capacity", "[tcp][peer-window]")
{
    Topology topology(2);
    topology.addLink(0, 1, 100.0, 1.0);
    SimulationEngine engine(topology);
    engine.startTCPConnection(0, 1);
    auto& session = engine.getTCPSession(0);
    session.getServerConnection().setReceiveWindow(0);
    for (int i = 0; i < 3; ++i) REQUIRE(engine.processEvent());
    REQUIRE(session.getClientConnection().getPeerWindow() == 0);
    REQUIRE_FALSE(session.hasPendingGeneration());
}
