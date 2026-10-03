#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>

#include "network/transport/tcp/congestion/NewRenoCongestionControl.hpp"

using namespace kns;

TEST_CASE(
    "NewReno starts with one MSS congestion window",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        64000
    );

    REQUIRE(control.getMss() == 1000);
    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 64000);
    REQUIRE_FALSE(control.inFastRecovery());
    REQUIRE_FALSE(control.partialAckObserved());
}

TEST_CASE(
    "NewReno slow start increases cwnd by acknowledged bytes up to one MSS",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        8000
    );

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 2000);

    control.onAck(500);

    REQUIRE(control.getCwnd() == 2500);

    control.onAck(2000);

    REQUIRE(control.getCwnd() == 3500);
}

TEST_CASE(
    "NewReno congestion avoidance increases cwnd using MSS squared over cwnd",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        2000
    );

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 2000);

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 2500);
}

TEST_CASE(
    "NewReno enters fast recovery after fast retransmit",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    REQUIRE(control.getSsthresh() == 5000);
    REQUIRE(control.getCwnd() == 8000);
    REQUIRE(control.inFastRecovery());
    REQUIRE_FALSE(control.partialAckObserved());
}

TEST_CASE(
    "NewReno inflates cwnd for additional duplicate ACKs",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    control.onDuplicateAck();

    REQUIRE(control.getCwnd() == 9000);

    control.onDuplicateAck();

    REQUIRE(control.getCwnd() == 10000);
    REQUIRE(control.inFastRecovery());
}

TEST_CASE(
    "NewReno keeps fast recovery active after a partial ACK",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    REQUIRE(control.getCwnd() == 8000);

    control.onRecoveryAck(1000, false);

    REQUIRE(control.getCwnd() == 8000);
    REQUIRE(control.inFastRecovery());
    REQUIRE(control.partialAckObserved());
}

TEST_CASE(
    "NewReno partially deflates cwnd when a partial ACK acknowledges less than MSS",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    control.onRecoveryAck(500, false);

    REQUIRE(control.getCwnd() == 7500);
    REQUIRE(control.inFastRecovery());
    REQUIRE(control.partialAckObserved());
}

TEST_CASE(
    "NewReno exits fast recovery on a full ACK",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    REQUIRE(control.inFastRecovery());

    control.onRecoveryAck(5000, true);

    REQUIRE(control.getCwnd() == 5000);
    REQUIRE_FALSE(control.inFastRecovery());
    REQUIRE_FALSE(control.partialAckObserved());
}

TEST_CASE(
    "NewReno timeout resets cwnd to one MSS",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);
    control.onDuplicateAck();

    control.onLoss();

    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 5000);
    REQUIRE_FALSE(control.inFastRecovery());
    REQUIRE_FALSE(control.partialAckObserved());
}

TEST_CASE(
    "NewReno reset restores the initial congestion state",
    "[tcp][congestion][newreno]"
)
{
    NewRenoCongestionControl control(
        1000,
        8000
    );

    control.onFastRetransmit(6000);
    control.onRecoveryAck(500, false);

    control.reset();

    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 8000);
    REQUIRE_FALSE(control.inFastRecovery());
    REQUIRE_FALSE(control.partialAckObserved());
}

TEST_CASE(
    "NewReno rejects a zero MSS",
    "[tcp][congestion][newreno]"
)
{
    REQUIRE_THROWS_AS(
        NewRenoCongestionControl(0),
        std::invalid_argument
    );
}