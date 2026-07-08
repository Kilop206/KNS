#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPSession.hpp"

using kns::TCPSession;
using kns::TCPState;

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
