#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>

#include "network/transport/tcp/congestion/RenoCongestionControl.hpp"

using namespace kns;

TEST_CASE(
    "Reno starts with one MSS congestion window",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
        1000,
        64000
    );

    REQUIRE(control.getMss() == 1000);
    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 64000);
    REQUIRE_FALSE(control.inFastRecovery());
}

TEST_CASE(
    "Reno slow start increases cwnd by acknowledged bytes up to one MSS",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
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
    "Reno congestion avoidance increases cwnd using MSS squared over cwnd",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
        1000,
        2000
    );

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 2000);

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 2500);
}

TEST_CASE(
    "Reno fast retransmit enters fast recovery",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
        1000,
        64000
    );

    control.onAck(1000);
    control.onAck(1000);
    control.onAck(1000);

    REQUIRE(control.getCwnd() == 4000);

    control.onFastRetransmit(4000);

    REQUIRE(control.getSsthresh() == 2000);
    REQUIRE(control.getCwnd() == 5000);
    REQUIRE(control.inFastRecovery());
}

TEST_CASE(
    "Reno duplicate ACKs inflate cwnd during fast recovery",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    REQUIRE(control.getSsthresh() == 5000);
    REQUIRE(control.getCwnd() == 8000);

    control.onDuplicateAck();

    REQUIRE(control.getCwnd() == 9000);

    control.onDuplicateAck();

    REQUIRE(control.getCwnd() == 10000);
    REQUIRE(control.inFastRecovery());
}

TEST_CASE(
    "Reno new ACK exits fast recovery at ssthresh",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    REQUIRE(control.inFastRecovery());

    control.onRecoveryAck(1000, true);

    REQUIRE(control.getCwnd() == 5000);
    REQUIRE_FALSE(control.inFastRecovery());
}

TEST_CASE(
    "Reno timeout loss resets cwnd to one MSS",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
        1000,
        64000
    );

    control.onAck(1000);
    control.onAck(1000);
    control.onAck(1000);

    REQUIRE(control.getCwnd() == 4000);

    control.onLoss();

    REQUIRE(control.getSsthresh() == 2000);
    REQUIRE(control.getCwnd() == 1000);
    REQUIRE_FALSE(control.inFastRecovery());
}

TEST_CASE(
    "Reno fast retransmit uses a two MSS minimum ssthresh",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(1000);

    REQUIRE(control.getSsthresh() == 2000);
    REQUIRE(control.getCwnd() == 5000);
}

TEST_CASE(
    "Reno ignores duplicate ACKs outside fast recovery",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
        1000,
        64000
    );

    control.onDuplicateAck();

    REQUIRE(control.getCwnd() == 1000);
    REQUIRE_FALSE(control.inFastRecovery());
}

TEST_CASE(
    "Reno reset restores the initial congestion state",
    "[tcp][congestion][reno]"
)
{
    RenoCongestionControl control(
        1000,
        8000
    );

    control.onAck(1000);
    control.onFastRetransmit(2000);
    control.onDuplicateAck();

    control.reset();

    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 8000);
    REQUIRE_FALSE(control.inFastRecovery());
}

TEST_CASE(
    "Reno rejects a zero MSS",
    "[tcp][congestion][reno]"
)
{
    REQUIRE_THROWS_AS(
        RenoCongestionControl(0),
        std::invalid_argument
    );
}