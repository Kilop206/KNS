#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPConnection.hpp"

using kns::TCPConnection;
using kns::TCPState;

TEST_CASE("TCPConnection accepts SYN only from valid initial states",
          "[tcp][send]")
{
    SECTION("closed state")
    {
        TCPConnection connection(TCPState::CLOSED, 100, 0, 1, 2);

        REQUIRE(connection.send_syn());
        REQUIRE(connection.getTcpState() == TCPState::SYN_SENT);
    }

    SECTION("listen state")
    {
        TCPConnection connection(TCPState::LISTEN, 100, 0, 1, 2);

        REQUIRE(connection.send_syn());
        REQUIRE(connection.getTcpState() == TCPState::SYN_SENT);
    }
}

TEST_CASE("TCPConnection rejects SYN from incompatible states",
          "[tcp][send]")
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        100,
        500,
        1,
        2
    );

    const auto sequence_before = connection.getSeqNum();

    REQUIRE_FALSE(connection.send_syn());
    REQUIRE(connection.getTcpState() == TCPState::ESTABLISHED);
    REQUIRE(connection.getSeqNum() == sequence_before);
}

TEST_CASE("TCPConnection validates SYN-ACK state",
          "[tcp][send]")
{
    SECTION("accepted in SYN_RECEIVED")
    {
        TCPConnection connection(
            TCPState::SYN_RECEIVED,
            500,
            101,
            2,
            1
        );

        REQUIRE(connection.send_syn_ack());
        REQUIRE(connection.getTcpState() == TCPState::SYN_RECEIVED);
    }

    SECTION("rejected in CLOSED")
    {
        TCPConnection connection(
            TCPState::CLOSED,
            500,
            101,
            2,
            1
        );

        REQUIRE_FALSE(connection.send_syn_ack());
        REQUIRE(connection.getTcpState() == TCPState::CLOSED);
    }

    SECTION("rejected in ESTABLISHED")
    {
        TCPConnection connection(
            TCPState::ESTABLISHED,
            500,
            101,
            2,
            1
        );

        REQUIRE_FALSE(connection.send_syn_ack());
        REQUIRE(connection.getTcpState() == TCPState::ESTABLISHED);
    }
}

TEST_CASE("TCPConnection allows ACK construction without changing state",
          "[tcp][send]")
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        100,
        500,
        1,
        2
    );

    const auto state_before = connection.getTcpState();
    const auto sequence_before = connection.getSeqNum();
    const auto expected_ack_before = connection.getExpectedAckNum();

    REQUIRE(connection.send_ack());

    REQUIRE(connection.getTcpState() == state_before);
    REQUIRE(connection.getSeqNum() == sequence_before);
    REQUIRE(connection.getExpectedAckNum() == expected_ack_before);
}

TEST_CASE("TCPConnection accepts FIN from valid closing states",
          "[tcp][send]")
{
    SECTION("established")
    {
        TCPConnection connection(
            TCPState::ESTABLISHED,
            100,
            500,
            1,
            2
        );

        REQUIRE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::FIN_WAIT_1);
    }

    SECTION("close wait")
    {
        TCPConnection connection(
            TCPState::CLOSE_WAIT,
            100,
            500,
            1,
            2
        );

        REQUIRE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::LAST_ACK);
    }
}

TEST_CASE("TCPConnection rejects FIN from incompatible states",
          "[tcp][send]")
{
    SECTION("closed")
    {
        TCPConnection connection(TCPState::CLOSED, 100, 500, 1, 2);

        REQUIRE_FALSE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::CLOSED);
    }

    SECTION("listen")
    {
        TCPConnection connection(TCPState::LISTEN, 100, 500, 1, 2);

        REQUIRE_FALSE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::LISTEN);
    }

    SECTION("SYN_SENT")
    {
        TCPConnection connection(TCPState::SYN_SENT, 100, 500, 1, 2);

        REQUIRE_FALSE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::SYN_SENT);
    }

    SECTION("SYN_RECEIVED")
    {
        TCPConnection connection(TCPState::SYN_RECEIVED, 100, 500, 1, 2);

        REQUIRE_FALSE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::SYN_RECEIVED);
    }

    SECTION("FIN_WAIT_1")
    {
        TCPConnection connection(TCPState::FIN_WAIT_1, 100, 500, 1, 2);

        REQUIRE_FALSE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::FIN_WAIT_1);
    }

    SECTION("FIN_WAIT_2")
    {
        TCPConnection connection(TCPState::FIN_WAIT_2, 100, 500, 1, 2);

        REQUIRE_FALSE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::FIN_WAIT_2);
    }

    SECTION("CLOSING")
    {
        TCPConnection connection(TCPState::CLOSING, 100, 500, 1, 2);

        REQUIRE_FALSE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::CLOSING);
    }

    SECTION("TIME_WAIT")
    {
        TCPConnection connection(TCPState::TIME_WAIT, 100, 500, 1, 2);

        REQUIRE_FALSE(connection.send_fin());
        REQUIRE(connection.getTcpState() == TCPState::TIME_WAIT);
    }
}