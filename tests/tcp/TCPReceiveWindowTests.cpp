#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPConnection.hpp"

using kns::TCPConnection;
using kns::TCPState;
using kns::TCPSegment;

TEST_CASE(
    "TCP receive buffer preserves ACK when segment arrives out of order",
    "[tcp][receive][window]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        100,
        0,
        1
    );

    const std::vector<std::uint8_t> second{
        0x42, 0x42, 0x42
    };

    REQUIRE(
        connection.receive_data(
            103,
            second,
            1.0
        )
    );

    REQUIRE(
        connection.getExpectedAckNum() == 100
    );

    REQUIRE(
        connection.getReceiveBufferSize() == 1
    );

    REQUIRE(
        connection.getReceiveBufferedBytes() == 3
    );
}

TEST_CASE(
    "TCP receive buffer advances cumulative ACK when gap is filled",
    "[tcp][receive][window]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        100,
        0,
        1
    );

    const std::vector<std::uint8_t> second{
        0x42, 0x42, 0x42
    };

    const std::vector<std::uint8_t> first{
        0x41, 0x41, 0x41
    };

    REQUIRE(
        connection.receive_data(
            103,
            second,
            1.0
        )
    );

    REQUIRE(
        connection.getExpectedAckNum() == 100
    );

    REQUIRE(
        connection.receive_data(
            100,
            first,
            2.0
        )
    );

    REQUIRE(
        connection.getExpectedAckNum() == 106
    );

    REQUIRE(
        connection.getReceiveBufferSize() == 0
    );

    REQUIRE(
        connection.getReceiveBufferedBytes() == 0
    );
}

TEST_CASE(
    "TCP receive buffer rejects duplicate segment",
    "[tcp][receive][window]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        100,
        0,
        1
    );

    const std::vector<std::uint8_t> payload{
        0x41, 0x41, 0x41
    };

    REQUIRE(
        connection.receive_data(
            103,
            payload,
            1.0
        )
    );

    REQUIRE_FALSE(
        connection.receive_data(
            103,
            payload,
            2.0
        )
    );

    REQUIRE(
        connection.getReceiveBufferSize() == 1
    );

    REQUIRE(
        connection.getExpectedAckNum() == 100
    );
}

TEST_CASE(
    "TCP ACK advertises available receive window",
    "[tcp][receive][window]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        100,
        0,
        1
    );

    TCPSegment ack = connection.buildAck();

    REQUIRE(
        ack.window == 65535
    );
}