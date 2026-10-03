#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/recovery/TCPLossDetector.hpp"

using kns::TCPLossDetector;

TEST_CASE(
    "TCPLossDetector starts without an observed ACK",
    "[tcp][loss-detector]"
)
{
    TCPLossDetector detector;

    REQUIRE_FALSE(
        detector.hasObservedAck()
    );

    REQUIRE(
        detector.getLastAck() == 0
    );

    REQUIRE(
        detector.getDuplicateAckCount() == 0
    );

    REQUIRE_FALSE(
        detector.shouldFastRetransmit()
    );
}

TEST_CASE(
    "TCPLossDetector records the first ACK",
    "[tcp][loss-detector]"
)
{
    TCPLossDetector detector;

    REQUIRE_FALSE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.hasObservedAck()
    );

    REQUIRE(
        detector.getLastAck() == 1000
    );

    REQUIRE(
        detector.getDuplicateAckCount() == 0
    );

    REQUIRE_FALSE(
        detector.shouldFastRetransmit()
    );
}

TEST_CASE(
    "TCPLossDetector counts duplicate ACKs",
    "[tcp][loss-detector]"
)
{
    TCPLossDetector detector;

    REQUIRE_FALSE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.getDuplicateAckCount() == 1
    );

    REQUIRE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.getDuplicateAckCount() == 2
    );

    REQUIRE_FALSE(
        detector.shouldFastRetransmit()
    );
}

TEST_CASE(
    "TCPLossDetector triggers fast retransmit after three duplicate ACKs",
    "[tcp][loss-detector]"
)
{
    TCPLossDetector detector;

    REQUIRE_FALSE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.observeAck(1000)
    );

    REQUIRE_FALSE(
        detector.shouldFastRetransmit()
    );

    REQUIRE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.getDuplicateAckCount() == 3
    );

    REQUIRE(
        detector.shouldFastRetransmit()
    );
}

TEST_CASE(
    "TCPLossDetector resets duplicate ACK streak when ACK advances",
    "[tcp][loss-detector]"
)
{
    TCPLossDetector detector;

    REQUIRE_FALSE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.observeAck(1000)
    );

    REQUIRE(
        detector.getDuplicateAckCount() == 2
    );

    REQUIRE_FALSE(
        detector.observeAck(1100)
    );

    REQUIRE(
        detector.getLastAck() == 1100
    );

    REQUIRE(
        detector.getDuplicateAckCount() == 0
    );

    REQUIRE_FALSE(
        detector.shouldFastRetransmit()
    );
}

TEST_CASE(
    "TCPLossDetector can trigger again after ACK advances",
    "[tcp][loss-detector]"
)
{
    TCPLossDetector detector;

    detector.observeAck(1000);
    detector.observeAck(1000);
    detector.observeAck(1000);
    detector.observeAck(1000);

    REQUIRE(
        detector.shouldFastRetransmit()
    );

    REQUIRE_FALSE(
        detector.observeAck(1100)
    );

    detector.observeAck(1100);
    detector.observeAck(1100);
    detector.observeAck(1100);

    REQUIRE(
        detector.shouldFastRetransmit()
    );
}

TEST_CASE(
    "TCPLossDetector reset restores initial state",
    "[tcp][loss-detector]"
)
{
    TCPLossDetector detector;

    detector.observeAck(1000);
    detector.observeAck(1000);
    detector.observeAck(1000);
    detector.observeAck(1000);

    REQUIRE(
        detector.shouldFastRetransmit()
    );

    detector.reset();

    REQUIRE_FALSE(
        detector.hasObservedAck()
    );

    REQUIRE(
        detector.getLastAck() == 0
    );

    REQUIRE(
        detector.getDuplicateAckCount() == 0
    );

    REQUIRE_FALSE(
        detector.shouldFastRetransmit()
    );
}