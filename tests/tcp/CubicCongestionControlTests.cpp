#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <stdexcept>

#include "network/transport/tcp/congestion/CubicCongestionControl.hpp"

using namespace kns;

TEST_CASE(
    "CUBIC starts with one MSS congestion window",
    "[tcp][congestion][cubic]"
)
{
    CubicCongestionControl control(
        1000,
        64000
    );

    REQUIRE(control.getMss() == 1000);
    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 64000);
    REQUIRE(control.getWMax() == 1000);
    REQUIRE(control.getBeta() == Catch::Approx(0.7));
    REQUIRE(control.getC() == Catch::Approx(0.4));
}

TEST_CASE(
    "CUBIC uses standard slow start below ssthresh",
    "[tcp][congestion][cubic]"
)
{
    CubicCongestionControl control(
        1000,
        4000
    );

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 2000);

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 3000);

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 4000);
}

TEST_CASE(
    "CUBIC loss applies beta reduction and stores WMax",
    "[tcp][congestion][cubic]"
)
{
    CubicCongestionControl control(
        1000,
        64000
    );

    control.onAck(1000);
    control.onAck(1000);
    control.onAck(1000);

    REQUIRE(control.getCwnd() == 4000);

    control.onLoss();

    REQUIRE(control.getWMax() == 4000);
    REQUIRE(control.getSsthresh() == 2800);
    REQUIRE(control.getCwnd() == 2800);
    REQUIRE_FALSE(control.inFastRecovery());
}

TEST_CASE(
    "CUBIC fast retransmit enters recovery using beta reduction",
    "[tcp][congestion][cubic]"
)
{
    CubicCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    REQUIRE(control.getWMax() == 1000);
    REQUIRE(control.getSsthresh() == 7000);
    REQUIRE(control.getCwnd() == 7000);
    REQUIRE(control.inFastRecovery());
}

TEST_CASE(
    "CUBIC duplicate ACKs increase cwnd during fast recovery",
    "[tcp][congestion][cubic]"
)
{
    CubicCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    control.onDuplicateAck();

    REQUIRE(control.getCwnd() == 8000);

    control.onDuplicateAck();

    REQUIRE(control.getCwnd() == 9000);
}

TEST_CASE(
    "CUBIC recovery ACK exits fast recovery",
    "[tcp][congestion][cubic]"
)
{
    CubicCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    control.onDuplicateAck();

    control.onRecoveryAck(
        1000,
        true
    );

    REQUIRE(control.getCwnd() == 7000);
    REQUIRE_FALSE(control.inFastRecovery());
}

TEST_CASE(
    "CUBIC recovery ACK may keep fast recovery active",
    "[tcp][congestion][cubic]"
)
{
    CubicCongestionControl control(
        1000,
        64000
    );

    control.onFastRetransmit(10000);

    control.onRecoveryAck(
        1000,
        false
    );

    REQUIRE(control.getCwnd() == 7000);
    REQUIRE(control.inFastRecovery());
}

TEST_CASE(
    "CUBIC ignores duplicate ACKs outside fast recovery",
    "[tcp][congestion][cubic]"
)
{
    CubicCongestionControl control(
        1000,
        64000
    );

    control.onDuplicateAck();

    REQUIRE(control.getCwnd() == 1000);
    REQUIRE_FALSE(control.inFastRecovery());
}

TEST_CASE(
    "CUBIC reset restores the initial congestion state",
    "[tcp][congestion][cubic]"
)
{
    CubicCongestionControl control(
        1000,
        8000
    );

    control.onAck(1000);
    control.onLoss();
    control.onFastRetransmit(10000);

    control.reset();

    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 8000);
    REQUIRE(control.getWMax() == 1000);
    REQUIRE_FALSE(control.inFastRecovery());
}

TEST_CASE(
    "CUBIC rejects invalid parameters",
    "[tcp][congestion][cubic]"
)
{
    REQUIRE_THROWS_AS(
        CubicCongestionControl(0),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        CubicCongestionControl(1000, 64000, 0.0),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        CubicCongestionControl(1000, 64000, 1.0),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        CubicCongestionControl(1000, 64000, 0.7, 0.0),
        std::invalid_argument
    );
}