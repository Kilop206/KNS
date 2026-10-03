#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/TCPSegment.hpp"

using kns::TCPConnection;
using kns::TCPSegment;
using kns::TCPState;
using kns::TCPFlag;

TEST_CASE(
    "TCPConnection counts duplicate ACKs",
    "[tcp][loss-detector][ack]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    TCPSegment segment;

    segment.seq = 1000;
    segment.payload.assign(
        100,
        0x41
    );
    segment.flags =
        TCPFlag::ACK |
        TCPFlag::PSH;

    REQUIRE(
        connection.queueSentSegment(
            segment,
            0.0
        )
    );

    /*
     * SND.UNA remains 1000 until the DATA is acknowledged.
     */
    REQUIRE(
        connection.getSendUnacknowledged() == 1000
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 0
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            1.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 1
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            1.1
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 2
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            1.2
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 3
    );

    REQUIRE(
        connection.shouldFastRetransmit()
    );
}

TEST_CASE(
    "TCPConnection resets duplicate ACK count when ACK advances",
    "[tcp][loss-detector][ack]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    TCPSegment segment;

    segment.seq = 1000;
    segment.payload.assign(
        100,
        0x41
    );
    segment.flags =
        TCPFlag::ACK |
        TCPFlag::PSH;

    REQUIRE(
        connection.queueSentSegment(
            segment,
            0.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            1.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            1.1
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 2
    );

    REQUIRE(
        connection.receive_ack(
            1100,
            2.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 0
    );

    REQUIRE_FALSE(
        connection.shouldFastRetransmit()
    );

    REQUIRE(
        connection.getSendUnacknowledged() == 1100
    );
}

TEST_CASE(
    "TCPConnection ignores stale ACKs below SND.UNA",
    "[tcp][loss-detector][ack]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    TCPSegment segment;

    segment.seq = 1000;
    segment.payload.assign(
        100,
        0x41
    );
    segment.flags =
        TCPFlag::ACK |
        TCPFlag::PSH;

    REQUIRE(
        connection.queueSentSegment(
            segment,
            0.0
        )
    );

    REQUIRE(
        connection.receive_ack(
            1100,
            1.0
        )
    );

    REQUIRE(
        connection.getSendUnacknowledged() == 1100
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            1.1
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 0
    );
}

TEST_CASE(
    "TCPConnection can reset loss detection state",
    "[tcp][loss-detector]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            1.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            1.1
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 2
    );

    connection.resetLossDetection();

    REQUIRE(
        connection.getDuplicateAckCount() == 0
    );

    REQUIRE_FALSE(
        connection.shouldFastRetransmit()
    );
}