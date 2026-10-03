#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPConnection.hpp"

using kns::TCPConnection;
using kns::TCPSegment;
using kns::TCPState;

TEST_CASE(
    "TCPConnection initializes send window state",
    "[tcp][window]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    REQUIRE(connection.getSendUnacknowledged() == 1000);
    REQUIRE(connection.getSendNext() == 1000);
    REQUIRE(
        connection.getSendWindow() ==
        TCPConnection::DEFAULT_SEND_WINDOW
    );
    REQUIRE(connection.getSendBufferSize() == 0);
}

TEST_CASE(
    "TCPConnection queues sent data and advances SND.NXT",
    "[tcp][window]"
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
    segment.payload.assign(100, 0x41);

    REQUIRE(
        connection.queueSentSegment(
            segment,
            10.0
        )
    );

    REQUIRE(connection.getSendBufferSize() == 1);
    REQUIRE(connection.getSendUnacknowledged() == 1000);
    REQUIRE(connection.getSendNext() == 1100);
}

TEST_CASE(
    "TCPConnection rejects data outside the send window",
    "[tcp][window]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    connection.setSendWindow(100);

    TCPSegment segment;
    segment.seq = 1000;
    segment.payload.assign(101, 0x41);

    REQUIRE_FALSE(
        connection.queueSentSegment(
            segment,
            10.0
        )
    );

    REQUIRE(connection.getSendBufferSize() == 0);
    REQUIRE(connection.getSendNext() == 1000);
}

TEST_CASE(
    "TCPConnection releases cumulatively acknowledged data",
    "[tcp][window]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    TCPSegment first;
    first.seq = 1000;
    first.payload.assign(100, 0x41);

    TCPSegment second;
    second.seq = 1100;
    second.payload.assign(100, 0x42);

    REQUIRE(
        connection.queueSentSegment(
            first,
            10.0
        )
    );

    REQUIRE(
        connection.queueSentSegment(
            second,
            11.0
        )
    );

    REQUIRE(connection.getSendBufferSize() == 2);
    REQUIRE(connection.getSendNext() == 1200);
    REQUIRE(connection.getSendUnacknowledged() == 1000);

    REQUIRE(connection.receive_ack(1100, 1.0));

    REQUIRE(connection.getSendBufferSize() == 1);
    REQUIRE(connection.getSendUnacknowledged() == 1100);

    REQUIRE(connection.receive_ack(1200, 1.0));

    REQUIRE(connection.getSendBufferSize() == 0);
    REQUIRE(connection.getSendUnacknowledged() == 1200);
}

TEST_CASE(
    "TCPConnection rejects invalid cumulative ACK",
    "[tcp][window]"
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
    segment.payload.assign(100, 0x41);

    REQUIRE(
        connection.queueSentSegment(
            segment,
            10.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_ack(1200, 1.0)
    );

    REQUIRE(
        connection.getSendBufferSize() == 1
    );

    REQUIRE(
        connection.getSendUnacknowledged() == 1000
    );

    REQUIRE_FALSE(
        connection.receive_ack(900, 1.0)
    );

    REQUIRE(
        connection.getSendBufferSize() == 1
    );

    REQUIRE(
        connection.getSendUnacknowledged() == 1000
    );
}

TEST_CASE(
    "TCPConnection accepts cumulative ACK equal to SND.NXT",
    "[tcp][window]"
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
    segment.payload.assign(100, 0x41);

    REQUIRE(
        connection.queueSentSegment(
            segment,
            10.0
        )
    );

    REQUIRE(
        connection.receive_ack(
            1100,
            1.0
        )
    );

    REQUIRE(
        connection.getSendBufferSize() == 0
    );

    REQUIRE(
        connection.getSendUnacknowledged() == 1100
    );

    REQUIRE(
        connection.getSendNext() == 1100
    );
}

TEST_CASE(
    "TCPConnection counts duplicate ACKs",
    "[tcp][loss-detection]"
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
    segment.payload.assign(100, 0x41);

    REQUIRE(
        connection.queueSentSegment(
            segment,
            10.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            11.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 1
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            12.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 2
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            13.0
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
    "[tcp][loss-detection]"
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
    segment.payload.assign(100, 0x41);

    REQUIRE(
        connection.queueSentSegment(
            segment,
            10.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            11.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            12.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 2
    );

    REQUIRE(
        connection.receive_ack(
            1100,
            13.0
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
    "TCPConnection ignores stale ACKs for loss detection",
    "[tcp][loss-detection]"
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
    segment.payload.assign(100, 0x41);

    REQUIRE(
        connection.queueSentSegment(
            segment,
            10.0
        )
    );

    REQUIRE(
        connection.receive_ack(
            1100,
            11.0
        )
    );

    REQUIRE(
        connection.getSendUnacknowledged() == 1100
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 0
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            12.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 0
    );

    REQUIRE_FALSE(
        connection.shouldFastRetransmit()
    );
}

TEST_CASE(
    "TCPConnection can reset loss detection state",
    "[tcp][loss-detection]"
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
    segment.payload.assign(100, 0x41);

    REQUIRE(
        connection.queueSentSegment(
            segment,
            10.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            11.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            12.0
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