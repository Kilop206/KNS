#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "core/SimulationClock.h"

using namespace gns::core;

TEST_CASE("SimulationClock initializes at zero") {
    SimulationClock clock(0.1);
    REQUIRE(clock.now() == 0.0);
}

TEST_CASE("SimulationClock advances correctly") {
    SimulationClock clock(0.5);

    clock.tick();
    REQUIRE(clock.now() == 0.5);

    clock.tick();
    REQUIRE(clock.now() == 1.0);
}

TEST_CASE("Multiple ticks accumulate deterministically") {
    SimulationClock clock(0.1);

    for (int i = 0; i < 10; ++i)
        clock.tick();

    REQUIRE(clock.now() == Catch::Approx(1.0));
}