#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "network/Topology.hpp"
#include "engine/events/TCPHandshakeTimeoutEvent.hpp"
#include "network/transport/tcp/TCPListener.hpp"

using namespace kns;

TEST_CASE("SYN loss preserves bounded handshake retries", "[tcp][handshake][loss]")
{
    Topology topology(2);
    auto link = topology.addLinkPtr(0, 1, 100.0, 1.0, 1.0);
    SimulationEngine engine(topology);
    engine.startTCPConnection(0, 1);
    const auto id = engine.getTCPSessions().begin()->first;
    REQUIRE(engine.processEvent());
    REQUIRE(engine.hasEvents());
    REQUIRE(engine.processEvent());
    REQUIRE(engine.getTCPSession(id).getClientConnection().getSynRetries() == 1);
    REQUIRE(engine.hasEvents());

    SECTION("Connectivity returns before retries are exhausted") {
        link->setLossProb(0.0);
        for (int i = 0; i < 10 &&
                engine.getTCPSession(id).getClientConnection().getTcpState() == TCPState::SYN_SENT; ++i) {
            REQUIRE(engine.processEvent());
        }
        REQUIRE(engine.getTCPSession(id).getClientConnection().getTcpState() == TCPState::ESTABLISHED);
    }
    SECTION("Permanent loss has a finite retry budget") {
        int events = 0;
        while (engine.hasEvents() && events < 100) {
            engine.processEvent();
            ++events;
        }
        REQUIRE(events < 100);
        REQUIRE_FALSE(engine.hasEvents());
        REQUIRE_FALSE(engine.getTCPSession(id).getClientConnection().canRetrySyn());
        REQUIRE(engine.getTCPSession(id).getState() == TCPState::CLOSED);
        REQUIRE(engine.getTCPSession(id).getFailureReason() ==
            TCPSession::FailureReason::SynRetriesExhausted);
    }
}

TEST_CASE("Exhausted handshake terminates endpoints and releases listener capacity", "[tcp][handshake][loss]")
{
    SimulationEngine engine(Topology(2));
    auto& listener = engine.startTCPListen(1, 1);
    const auto id = engine.acceptOnListener(1, 0, 1000);
    REQUIRE(id != TCPListener::INVALID_SESSION_ID);
    auto& session = engine.getTCPSession(id);
    auto& client = session.getClientConnection();
    client = TCPConnection(TCPState::SYN_SENT, 1000, 0, 0, 1);
    REQUIRE_FALSE(session.failHandshake());
    REQUIRE(session.getFailureReason() == TCPSession::FailureReason::None);
    while (client.canRetrySyn()) {
        client.incrementSynRetries();
    }
    TCPHandshakeTimeoutEvent timeout(0.0, id);
    timeout.execute(engine);
    REQUIRE(client.getTcpState() == TCPState::CLOSED);
    REQUIRE(session.getServerConnection().getTcpState() == TCPState::CLOSED);
    REQUIRE(session.getFailureReason() == TCPSession::FailureReason::SynRetriesExhausted);
    REQUIRE(listener.getActiveConnections() == 0);
    REQUIRE_FALSE(engine.hasEvents());
    REQUIRE_FALSE(engine.validateSimulation().sessions_ok);
    timeout.execute(engine);
    REQUIRE_FALSE(engine.hasEvents());
    REQUIRE(session.getFailureReason() == TCPSession::FailureReason::SynRetriesExhausted);
    REQUIRE(engine.acceptOnListener(1, 0, 2000) != TCPListener::INVALID_SESSION_ID);
}
