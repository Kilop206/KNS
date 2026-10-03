#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPConnectionCloseEvent.hpp"

using namespace kns;

TEST_CASE("Close recovers independently lost FINs and final ACK", "[tcp][close][loss]")
{
    for (int loss_position : {0, 1, 2, 3}) {
        CAPTURE(loss_position);
        Topology topology(2);
        topology.addLink(0, 1, 100.0, 1.0);
        SimulationEngine engine(topology);
        auto& listener = engine.startTCPListen(1, 1);
        const auto id = engine.acceptOnListener(1, 0, 100);
        REQUIRE(id != TCPListener::INVALID_SESSION_ID);
        auto& session = engine.getTCPSession(id);
        auto& client = session.getClientConnection();
        auto& server = session.getServerConnection();
        client = TCPConnection(TCPState::ESTABLISHED, 100, 500, 0, 1);
        server = TCPConnection(TCPState::ESTABLISHED, 500, 100, 1, 0);
        REQUIRE(listener.getActiveConnections() == 1);
        if (loss_position == 0 || loss_position == 3) engine.setGlobalLossProb(1.0f);
        TCPConnectionCloseEvent(0.0, id).execute(engine);
        if (loss_position == 0) engine.clearGlobalLossOverride();
        if (loss_position == 1) {
            engine.setPacketObserver([&](const Packet& packet, auto, int from, auto, auto, auto) {
                if (from == 1 && packet.packet_type == PacketType::ACK)
                    engine.setGlobalLossProb(1.0f);
            });
            REQUIRE(engine.processEvent());
            engine.setPacketObserver(nullptr);
            engine.clearGlobalLossOverride();
        }
        if (loss_position == 2) {
            for (int i = 0; i < 10 && client.getTcpState() != TCPState::FIN_WAIT_2; ++i)
                REQUIRE(engine.processEvent());
            REQUIRE(client.getTcpState() == TCPState::FIN_WAIT_2);
            engine.setGlobalLossProb(1.0f);
            REQUIRE(engine.processEvent());
            REQUIRE(client.getTcpState() == TCPState::TIME_WAIT);
            engine.clearGlobalLossOverride();
        }
        engine.run();
        REQUIRE(client.isClosed());
        REQUIRE(server.isClosed());
        REQUIRE(listener.getActiveConnections() == 0);
        REQUIRE(engine.getPacketsInTransit().empty());
        REQUIRE(engine.now() < 15.0);
        if (loss_position == 3) {
            REQUIRE(session.getFailureReason() == TCPSession::FailureReason::CloseRetriesExhausted);
            REQUIRE(engine.getStats().packets_lost == 6);
        } else {
            REQUIRE(session.getFailureReason() == TCPSession::FailureReason::None);
            REQUIRE(engine.getStats().packets_lost == 1);
        }
    }
}
