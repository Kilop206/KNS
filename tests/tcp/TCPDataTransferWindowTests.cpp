#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/TCPSegment.hpp"

using kns::TCPConnection;
using kns::TCPFlag;
using kns::TCPSegment;
using kns::TCPState;

TEST_CASE(
    "TCP send window blocks data when full",
    "[tcp][window][integration]"
)
{
    TCPConnection client(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    client.setSendWindow(100);

    REQUIRE(client.getSendUnacknowledged() == 1000);
    REQUIRE(client.getSendNext() == 1000);
    REQUIRE(client.getSendBufferSize() == 0);

    TCPSegment first;
    first.seq = 1000;
    first.payload.assign(100, 0x41);
    first.flags = TCPFlag::ACK | TCPFlag::PSH;

    REQUIRE(
        client.queueSentSegment(
            first,
            0.0
        )
    );

    REQUIRE(client.getSendUnacknowledged() == 1000);
    REQUIRE(client.getSendNext() == 1100);
    REQUIRE(client.getSendBufferSize() == 1);

    REQUIRE_FALSE(client.canSend(1));
}

TEST_CASE(
    "TCP cumulative ACK reopens send window",
    "[tcp][window][integration]"
)
{
    TCPConnection client(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    client.setSendWindow(200);

    TCPSegment first;
    first.seq = 1000;
    first.payload.assign(100, 0x41);
    first.flags = TCPFlag::ACK | TCPFlag::PSH;

    TCPSegment second;
    second.seq = 1100;
    second.payload.assign(100, 0x42);
    second.flags = TCPFlag::ACK | TCPFlag::PSH;

    REQUIRE(
        client.queueSentSegment(
            first,
            0.0
        )
    );

    REQUIRE(
        client.queueSentSegment(
            second,
            0.0
        )
    );

    REQUIRE(client.getSendUnacknowledged() == 1000);
    REQUIRE(client.getSendNext() == 1200);
    REQUIRE(client.getSendBufferSize() == 2);

    // Both segments occupy the complete 200-byte send window.
    REQUIRE_FALSE(client.canSend(1));

    // Cumulative ACK for the first 100 bytes.
    REQUIRE(client.receive_ack(1100, 1.0));

    REQUIRE(client.getSendUnacknowledged() == 1100);
    REQUIRE(client.getSendNext() == 1200);
    REQUIRE(client.getSendBufferSize() == 1);

    // Exactly 100 bytes are available again.
    REQUIRE(client.canSend(100));

    // One additional byte would exceed the remaining window.
    REQUIRE_FALSE(client.canSend(101));
}