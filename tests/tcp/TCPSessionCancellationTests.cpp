#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPTimeWaitTimeoutEvent.hpp"
#include "enums/LinkMode.hpp"
#include "network/Topology.hpp"
#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/TCPListener.hpp"

using kns::LinkMode;
using kns::SimulationEngine;
using kns::TCPConnection;
using kns::TCPListener;
using kns::TCPState;
using kns::TCPTimeWaitTimeoutEvent;
using kns::Topology;

TEST_CASE("Listener cleanup rejects nonterminal sessions without mutation", "[tcp][session][cleanup]")
{
    SimulationEngine engine(Topology(2));
    auto& listener = engine.startTCPListen(1, 1);
    const auto id = engine.acceptOnListener(1, 0, 1000);
    REQUIRE(id != TCPListener::INVALID_SESSION_ID);
    auto& session = engine.getTCPSession(id);
    auto& client = session.getClientConnection();
    auto& server = session.getServerConnection();
    client = TCPConnection(TCPState::ESTABLISHED, 100, 0, 0, 1);
    server = TCPConnection(TCPState::ESTABLISHED, 200, 0, 1, 0);
    kns::TCPSegment segment;
    segment.seq = client.getSendNext();
    segment.payload = {1, 2, 3};
    REQUIRE(client.queueSentSegment(segment, 0.0));
    const auto buffered = client.getSendBufferSize();
    REQUIRE(buffered > 0);
    REQUIRE_FALSE(engine.releaseTCPListenerSession(id));
    REQUIRE(client.getSendBufferSize() == buffered);
    REQUIRE(client.getTcpState() == TCPState::ESTABLISHED);
    REQUIRE(listener.getActiveConnections() == 1);
    REQUIRE(engine.acceptOnListener(1, 0, 3000) == TCPListener::INVALID_SESSION_ID);
    REQUIRE_FALSE(engine.releaseTCPListenerSession(TCPListener::INVALID_SESSION_ID));

    REQUIRE(server.failRetransmission());
    REQUIRE_FALSE(engine.releaseTCPListenerSession(id));
    REQUIRE(client.getSendBufferSize() == buffered);
    REQUIRE(listener.getActiveConnections() == 1);

    REQUIRE(client.failRetransmission());
    REQUIRE(engine.releaseTCPListenerSession(id));
    REQUIRE(listener.getActiveConnections() == 0);
    REQUIRE(client.getSendBufferSize() == 0);
    REQUIRE(engine.releaseTCPListenerSession(id));
    REQUIRE(engine.hasTCPSession(id));
}

TEST_CASE(
    "SimulationEngine cancels a TCP session and frees listener capacity",
    "[tcp][session][cancel]"
)
{
    Topology topology(2);
    topology.addLink(0, 1, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topology);
    auto& listener = engine.startTCPListen(1, 1);

    const auto session_id = engine.acceptOnListener(1, 0, 1000);

    REQUIRE(session_id != TCPListener::INVALID_SESSION_ID);
    REQUIRE(listener.getActiveConnections() == 1);
    REQUIRE(engine.hasTCPSession(session_id));

    REQUIRE(engine.cancelTCPSession(session_id));
    REQUIRE_FALSE(engine.hasTCPSession(session_id));
    REQUIRE(listener.getActiveConnections() == 0);
    REQUIRE_FALSE(engine.cancelTCPSession(session_id));
}

TEST_CASE(
    "Canceled TCP sessions make pending time-wait events harmless",
    "[tcp][session][cancel][event]"
)
{
    Topology topology(2);
    topology.addLink(0, 1, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topology);
    auto& session = engine.createTCPSession(0, 1);
    const auto session_id = session.getSession_id();

    session.getClientConnection() = TCPConnection(
        TCPState::TIME_WAIT,
        0,
        0,
        0,
        1
    );

    engine.schedule(
        std::make_unique<TCPTimeWaitTimeoutEvent>(0.0, session_id)
    );

    REQUIRE(engine.cancelTCPSession(session_id));
    REQUIRE(engine.processEvent());
    REQUIRE_FALSE(engine.hasTCPSession(session_id));
}
