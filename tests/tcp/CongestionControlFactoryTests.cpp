#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "network/transport/tcp/congestion/CongestionControlFactory.hpp"
#include "network/transport/tcp/congestion/CubicCongestionControl.hpp"
#include "network/transport/tcp/congestion/NewRenoCongestionControl.hpp"
#include "network/transport/tcp/congestion/RenoCongestionControl.hpp"
#include "network/transport/tcp/congestion/TahoeCongestionControl.hpp"

using namespace kns;

TEST_CASE(
    "CongestionControlFactory creates Tahoe",
    "[tcp][congestion][factory]"
)
{
    auto control = CongestionControlFactory::create(
        CongestionControlType::TAHOE,
        1000,
        64000
    );

    REQUIRE(control != nullptr);
    REQUIRE(control->getMss() == 1000);
    REQUIRE(control->getCwnd() == 1000);

    REQUIRE(
        dynamic_cast<TahoeCongestionControl*>(control.get()) != nullptr
    );
}

TEST_CASE(
    "CongestionControlFactory creates Reno",
    "[tcp][congestion][factory]"
)
{
    auto control = CongestionControlFactory::create(
        CongestionControlType::RENO,
        1000,
        64000
    );

    REQUIRE(control != nullptr);
    REQUIRE(control->getMss() == 1000);
    REQUIRE(control->getCwnd() == 1000);

    REQUIRE(
        dynamic_cast<RenoCongestionControl*>(control.get()) != nullptr
    );
}

TEST_CASE(
    "CongestionControlFactory creates NewReno",
    "[tcp][congestion][factory]"
)
{
    auto control = CongestionControlFactory::create(
        CongestionControlType::NEW_RENO,
        1000,
        64000
    );

    REQUIRE(control != nullptr);
    REQUIRE(control->getMss() == 1000);
    REQUIRE(control->getCwnd() == 1000);

    REQUIRE(
        dynamic_cast<NewRenoCongestionControl*>(control.get()) != nullptr
    );
}

TEST_CASE(
    "CongestionControlFactory creates CUBIC",
    "[tcp][congestion][factory]"
)
{
    auto control = CongestionControlFactory::create(
        CongestionControlType::CUBIC,
        1000,
        64000
    );

    REQUIRE(control != nullptr);
    REQUIRE(control->getMss() == 1000);
    REQUIRE(control->getCwnd() == 1000);

    REQUIRE(
        dynamic_cast<CubicCongestionControl*>(control.get()) != nullptr
    );
}

TEST_CASE(
    "CongestionControlFactory preserves the initial ssthresh",
    "[tcp][congestion][factory]"
)
{
    constexpr std::uint32_t initial_ssthresh = 12000;

    auto control = CongestionControlFactory::create(
        CongestionControlType::RENO,
        1000,
        initial_ssthresh
    );

    REQUIRE(control->getSsthresh() == initial_ssthresh);
}

TEST_CASE(
    "CongestionControlFactory rejects zero MSS",
    "[tcp][congestion][factory]"
)
{
    REQUIRE_THROWS_AS(
        CongestionControlFactory::create(
            CongestionControlType::TAHOE,
            0
        ),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        CongestionControlFactory::create(
            CongestionControlType::RENO,
            0
        ),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        CongestionControlFactory::create(
            CongestionControlType::NEW_RENO,
            0
        ),
        std::invalid_argument
    );

    REQUIRE_THROWS_AS(
        CongestionControlFactory::create(
            CongestionControlType::CUBIC,
            0
        ),
        std::invalid_argument
    );
}