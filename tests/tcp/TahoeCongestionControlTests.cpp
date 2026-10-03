#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>

#include "network/transport/tcp/congestion/TahoeCongestionControl.hpp"

using namespace kns;

TEST_CASE(
    "Tahoe starts with one MSS congestion window",
    "[tcp][congestion][tahoe]"
)
{
    TahoeCongestionControl control(
        1000,
        64000
    );

    REQUIRE(control.getMss() == 1000);
    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 64000);
}

TEST_CASE(
    "Tahoe slow start increases cwnd by acknowledged bytes up to one MSS",
    "[tcp][congestion][tahoe]"
)
{
    TahoeCongestionControl control(
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
    "Tahoe congestion avoidance increases cwnd using MSS squared over cwnd",
    "[tcp][congestion][tahoe]"
)
{
    TahoeCongestionControl control(
        1000,
        2000
    );

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 2000);

    control.onAck(1000);

    REQUIRE(control.getCwnd() == 2500);
}

TEST_CASE(
    "Tahoe loss halves ssthresh with a two MSS floor and resets cwnd",
    "[tcp][congestion][tahoe]"
)
{
    TahoeCongestionControl control(
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
}

TEST_CASE(
    "Tahoe loss keeps ssthresh at two MSS when cwnd is small",
    "[tcp][congestion][tahoe]"
)
{
    TahoeCongestionControl control(
        1000,
        64000
    );

    control.onLoss();

    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 2000);
}

TEST_CASE(
    "Tahoe canSend enforces the congestion window",
    "[tcp][congestion][tahoe]"
)
{
    TahoeCongestionControl control(
        1000,
        64000
    );

    REQUIRE(control.canSend(0, 1000));
    REQUIRE_FALSE(control.canSend(1, 1000));
    REQUIRE(control.canSend(1000, 0));
    REQUIRE_FALSE(control.canSend(1001, 0));
}

TEST_CASE(
    "Tahoe reset restores the initial congestion state",
    "[tcp][congestion][tahoe]"
)
{
    TahoeCongestionControl control(
        1000,
        8000
    );

    control.onAck(1000);
    control.onAck(1000);
    control.onLoss();

    REQUIRE(control.getCwnd() == 1000);

    control.reset();

    REQUIRE(control.getCwnd() == 1000);
    REQUIRE(control.getSsthresh() == 8000);
}

TEST_CASE(
    "Tahoe rejects a zero MSS",
    "[tcp][congestion][tahoe]"
)
{
    REQUIRE_THROWS_AS(
        TahoeCongestionControl(0),
        std::invalid_argument
    );
}