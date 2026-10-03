#include <catch2/catch_test_macros.hpp>

#include <map>

#include "engine/core/SimulationEngine.hpp"
#include "network/Topology.hpp"
#include "network/Link.hpp"
#include "network/transport/tcp/TCPSession.hpp"

using kns::LinkMode;
using kns::SimulationEngine;
using kns::TCPSession;
using kns::TCPState;
using kns::Topology;

TEST_CASE("TCPSession tracks packet completion", "[tcp][session]")
{
    TCPSession session(42, 1, 2, TCPState::ESTABLISHED);

    session.setTotalPackets(2);
    REQUIRE_FALSE(session.isComplete());

    session.incrementPacketsSent();
    REQUIRE_FALSE(session.isComplete());

    session.incrementPacketsSent();
    REQUIRE(session.isComplete());
}

TEST_CASE("TCPSession owns client and server endpoints", "[tcp][session]")
{
    TCPSession session(42, 1, 2, TCPState::CLOSED);

    REQUIRE(session.getSession_id() == 42);
    REQUIRE(session.getSource() == 1);
    REQUIRE(session.getDestination() == 2);
    REQUIRE(session.getClientConnection().getLocalNode() == 1);
    REQUIRE(session.getClientConnection().getRemoteNode() == 2);
    REQUIRE(session.getServerConnection().getLocalNode() == 2);
    REQUIRE(session.getServerConnection().getRemoteNode() == 1);
}

TEST_CASE("TCPSession default constructor is defined", "[tcp][session]")
{
    TCPSession session;

    REQUIRE(session.getSession_id() == 0);
    REQUIRE(session.getSource() == 0);
    REQUIRE(session.getDestination() == 0);
    REQUIRE(session.getState() == TCPState::CLOSED);

    // operator[] default-inserts; this was a link error when TCPSession()
    // was declared in the header but never defined.
    std::map<int, TCPSession> sessions;
    sessions[7];

    REQUIRE(sessions.at(7).getSession_id() == 0);
    REQUIRE(sessions.at(7).getState() == TCPState::CLOSED);
}

TEST_CASE(
    "Simulation generates DATA packets after TCP handshake",
    "[tcp][data][integration]"
)
{
    Topology topology(2);

    auto link = topology.addLinkPtr(
        0,
        1,
        100.0,
        1.0,
        0.0,
        LinkMode::FULL_DUPLEX
    );

    REQUIRE(link != nullptr);

    SimulationEngine engine(topology);

    engine.setGlobalPacketSize(1000);
    engine.startTCPConnection(0, 1);

    engine.run();

    const auto& stats = engine.getStats();

    REQUIRE(stats.packets_sent >= 20);
    REQUIRE(stats.packets_delivered >= 20);
    REQUIRE(stats.packets_lost == 0);
}