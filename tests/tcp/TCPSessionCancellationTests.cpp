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
