#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/buffer/TCPSendBuffer.hpp"
#include "network/transport/tcp/buffer/TCPReceiveBuffer.hpp"

using kns::TCPReceiveBuffer;
using kns::TCPReceiveEntry;
using kns::TCPSegment;
using kns::TCPSendBuffer;
using kns::TCPSendEntry;

TEST_CASE(
    "TCPSendBuffer preserves insertion order",
    "[tcp][buffer]"
)
{
    TCPSendBuffer buffer;

    TCPSegment first;
    first.seq = 100;
    first.payload.assign(10, 0x41);

    TCPSegment second;
    second.seq = 110;
    second.payload.assign(10, 0x42);

    REQUIRE(buffer.push(TCPSendEntry{first, 1.0}));
    REQUIRE(buffer.push(TCPSendEntry{second, 2.0}));

    REQUIRE(buffer.size() == 2);
    REQUIRE(buffer.front() != nullptr);
    REQUIRE(buffer.front()->segment.seq == 100);
}

TEST_CASE(
    "TCPSendBuffer releases cumulatively acknowledged entries",
    "[tcp][buffer]"
)
{
    TCPSendBuffer buffer;

    TCPSegment first;
    first.seq = 100;
    first.payload.assign(10, 0x41);

    TCPSegment second;
    second.seq = 110;
    second.payload.assign(10, 0x42);

    TCPSegment third;
    third.seq = 120;
    third.payload.assign(10, 0x43);

    REQUIRE(buffer.push(TCPSendEntry{first, 1.0}));
    REQUIRE(buffer.push(TCPSendEntry{second, 2.0}));
    REQUIRE(buffer.push(TCPSendEntry{third, 3.0}));

    REQUIRE(buffer.acknowledge(120) == 2);
    REQUIRE(buffer.size() == 1);
    REQUIRE(buffer.front()->segment.seq == 120);
}

TEST_CASE(
    "TCPSendBuffer enforces capacity",
    "[tcp][buffer]"
)
{
    TCPSendBuffer buffer(2);

    TCPSegment segment;
    segment.seq = 100;
    segment.payload.assign(10, 0x41);

    REQUIRE(buffer.push(TCPSendEntry{segment, 1.0}));
    REQUIRE(buffer.push(TCPSendEntry{segment, 2.0}));
    REQUIRE_FALSE(buffer.push(TCPSendEntry{segment, 3.0}));

    REQUIRE(buffer.size() == 2);
}

TEST_CASE(
    "TCPReceiveBuffer orders segments by sequence",
    "[tcp][buffer]"
)
{
    TCPReceiveBuffer buffer(100);

    TCPSegment third;
    third.seq = 120;
    third.payload.assign(10, 0x43);

    TCPSegment first;
    first.seq = 100;
    first.payload.assign(10, 0x41);

    TCPSegment second;
    second.seq = 110;
    second.payload.assign(10, 0x42);

    REQUIRE(buffer.push(TCPReceiveEntry{third, 3.0}));
    REQUIRE(buffer.push(TCPReceiveEntry{first, 1.0}));
    REQUIRE(buffer.push(TCPReceiveEntry{second, 2.0}));

    REQUIRE(buffer.size() == 3);
    REQUIRE(buffer.front() != nullptr);
    REQUIRE(buffer.front()->segment.seq == 100);
}

TEST_CASE(
    "TCPReceiveBuffer consumes contiguous segments",
    "[tcp][buffer]"
)
{
    TCPReceiveBuffer buffer(100);

    TCPSegment third;
    third.seq = 120;
    third.payload.assign(10, 0x43);

    TCPSegment first;
    first.seq = 100;
    first.payload.assign(10, 0x41);

    TCPSegment second;
    second.seq = 110;
    second.payload.assign(10, 0x42);

    REQUIRE(buffer.push(TCPReceiveEntry{third, 3.0}));
    REQUIRE(buffer.push(TCPReceiveEntry{first, 1.0}));
    REQUIRE(buffer.push(TCPReceiveEntry{second, 2.0}));

    REQUIRE(buffer.consumeContiguous() == 3);
    REQUIRE(buffer.empty());
    REQUIRE(buffer.nextSequence() == 130);
}

TEST_CASE(
    "TCPReceiveBuffer rejects duplicate and already consumed segments",
    "[tcp][buffer]"
)
{
    TCPReceiveBuffer buffer(100);

    TCPSegment segment;
    segment.seq = 100;
    segment.payload.assign(10, 0x41);

    REQUIRE(buffer.push(TCPReceiveEntry{segment, 1.0}));
    REQUIRE_FALSE(buffer.push(TCPReceiveEntry{segment, 2.0}));

    REQUIRE(buffer.consumeContiguous() == 1);
    REQUIRE(buffer.nextSequence() == 110);

    REQUIRE_FALSE(buffer.push(TCPReceiveEntry{segment, 3.0}));
}