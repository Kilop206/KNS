#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "network/transport/tcp/buffer/TCPSendBuffer.hpp"

using kns::TCPSegment;
using kns::TCPSendBuffer;
using kns::TCPSendEntry;

namespace {

    constexpr double EPSILON = 1e-9;

    bool almostEqual(
        double a,
        double b
    ) noexcept
    {
        return std::abs(a - b) <= EPSILON;
    }

}

TEST_CASE(
    "TCPSendBuffer provides RTT sample for original transmission",
    "[tcp][rto][karn]"
)
{
    TCPSendBuffer buffer;

    TCPSegment segment;

    segment.seq = 1000;
    segment.payload.assign(
        100,
        0x41
    );

    REQUIRE(
        buffer.push(
            TCPSendEntry{
                segment,
                10.0,
                false
            }
        )
    );

    const auto sample =
        buffer.acknowledgeAndGetRtt(
            1100,
            11.5
        );

    REQUIRE(sample.has_value());

    REQUIRE(
        almostEqual(
            *sample,
            1.5
        )
    );

    REQUIRE(buffer.empty());
}

TEST_CASE(
    "TCPSendBuffer rejects RTT sample after retransmission",
    "[tcp][rto][karn]"
)
{
    TCPSendBuffer buffer;

    TCPSegment segment;

    segment.seq = 1000;
    segment.payload.assign(
        100,
        0x41
    );

    REQUIRE(
        buffer.push(
            TCPSendEntry{
                segment,
                10.0,
                false
            }
        )
    );

    REQUIRE(
        buffer.markRetransmitted(
            1000,
            12.0
        )
    );

    const auto sample =
        buffer.acknowledgeAndGetRtt(
            1100,
            13.0
        );

    REQUIRE_FALSE(
        sample.has_value()
    );

    REQUIRE(buffer.empty());
}

TEST_CASE(
    "TCPSendBuffer can find an original RTT sample after retransmitted entry",
    "[tcp][rto][karn]"
)
{
    TCPSendBuffer buffer;

    TCPSegment retransmitted;
    retransmitted.seq = 1000;
    retransmitted.payload.assign(
        100,
        0x41
    );

    TCPSegment original;
    original.seq = 1100;
    original.payload.assign(
        100,
        0x42
    );

    REQUIRE(
        buffer.push(
            TCPSendEntry{
                retransmitted,
                10.0,
                true
            }
        )
    );

    REQUIRE(
        buffer.push(
            TCPSendEntry{
                original,
                10.5,
                false
            }
        )
    );

    const auto sample =
        buffer.acknowledgeAndGetRtt(
            1200,
            12.0
        );

    REQUIRE(sample.has_value());

    REQUIRE(
        almostEqual(
            *sample,
            1.5
        )
    );

    REQUIRE(buffer.empty());
}

TEST_CASE(
    "TCPSendEntry records retransmission state",
    "[tcp][rto][karn]"
)
{
    TCPSegment segment;

    segment.seq = 500;
    segment.payload.assign(
        50,
        0x41
    );

    TCPSendEntry entry{
        segment,
        2.0,
        false
    };

    REQUIRE_FALSE(
        entry.retransmitted
    );

    REQUIRE(
        entry.canMeasureRtt()
    );

    entry.markRetransmitted(4.0);

    REQUIRE(
        entry.retransmitted
    );

    REQUIRE_FALSE(
        entry.canMeasureRtt()
    );

    REQUIRE(
        almostEqual(
            entry.sent_at,
            4.0
        )
    );
}