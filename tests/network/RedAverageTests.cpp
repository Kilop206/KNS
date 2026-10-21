#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "network/Topology.hpp"
#include "engine/core/SimulationEngine.hpp"

using namespace kns;

TEST_CASE("RED EWMA smooths bursts without mutating on inspection", "[network][red][ewma]")
{
    Link link(0, 1, 100, 1, 0, LinkMode::FULL_DUPLEX, 16);
    link.configureRed(2, 6, 0.5, 0.25);
    for (int i = 0; i < 4; ++i) link.enqueueTransmission(0, 1, i, i + 10);
    REQUIRE(link.sampleEarlyDropProbability(0, 1, 0, 1) == 0.0);
    REQUIRE(link.getAverageQueueSize(0, 1) == 1.0);
    REQUIRE(link.earlyDropProbability(0, 1) == 0.0);
    REQUIRE(link.getAverageQueueSize(0, 1) == 1.0);
    REQUIRE(link.sampleEarlyDropProbability(0, 1, 0, 1) == 0.0);
    REQUIRE(link.sampleEarlyDropProbability(0, 1, 0, 1) == Catch::Approx(0.0390625));
    REQUIRE(link.getAverageQueueSize(0, 1) == 2.3125);
    REQUIRE(link.getAverageQueueSize(1, 0) == 0.0);
}

TEST_CASE("RED EWMA decays after a queue drains", "[network][red][ewma]")
{
    Link link(0, 1, 100, 1);
    link.configureRed(0, 8, 0.5, 0.25);
    for (int i = 0; i < 4; ++i) link.enqueueTransmission(0, 1, i, i + 10);
    link.sampleEarlyDropProbability(0, 1, 0, 1);
    for (int i = 0; i < 4; ++i) REQUIRE(link.dequeueTransmission(0, 1, i, i + 10));
    link.sampleEarlyDropProbability(0, 1, 23, 1);
    REQUIRE(link.getAverageQueueSize(0, 1) == Catch::Approx(std::pow(0.75, 11)));
}

TEST_CASE("RED EWMA shares half-duplex samples and validates weight atomically", "[network][red][ewma]")
{
    Link link(0, 1, 100, 1, 0, LinkMode::HALF_DUPLEX);
    link.configureRed(0, 8, 0.5, 0.5);
    link.enqueueTransmission(0, 1, 0, 1);
    link.enqueueTransmission(1, 0, 1, 2);
    link.sampleEarlyDropProbability(1, 0, 0, 1);
    REQUIRE(link.getAverageQueueSize(0, 1) == 1.0);
    REQUIRE(link.getAverageQueueSize(1, 0) == 1.0);
    for (double weight : {0.0, -0.1, 1.1, std::numeric_limits<double>::quiet_NaN()}) {
        REQUIRE_THROWS_AS(link.configureRed(1, 4, 0.25, weight), std::invalid_argument);
        REQUIRE(link.getRedQueueWeight() == 0.5);
        REQUIRE(link.getRedMinThreshold() == 0);
        REQUIRE(link.getAverageQueueSize(0, 1) == 1.0);
    }
}

TEST_CASE("RED EWMA configuration survives copies while runs start without history", "[network][red][ewma]")
{
    Topology topology(2);
    Link configured(0, 1, 100, 1);
    configured.configureRed(0, 8, 0.5, 0.5);
    topology.addLink(configured);
    auto link = topology.getLinks().front();
    link->enqueueTransmission(0, 1, 0, 1);
    link->sampleEarlyDropProbability(0, 1, 0, 1);
    REQUIRE(link->getAverageQueueSize(0, 1) == 0.5);
    SimulationEngine engine(topology);
    const auto runLink = engine.getTopology().getLinks().front();
    REQUIRE(runLink->getRedQueueWeight() == 0.5);
    REQUIRE(runLink->getAverageQueueSize(0, 1) == 0.0);
    REQUIRE(runLink->getQueueSize() == 0);
    REQUIRE(link->getAverageQueueSize(0, 1) == 0.5);
}
