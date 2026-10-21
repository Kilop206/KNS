#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "engine/core/RunConfig.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/Topology.hpp"

using namespace kns;

TEST_CASE("Rejected run configuration preserves packet size controller and RNG", "[tcp][congestion][run-config]")
{
    SimulationEngine engine(Topology(2));
    SimulationEngine control(Topology(2));
    RunConfig original;
    original.packet_size = 1200;
    original.seed = 97;
    original.congestion_control = CongestionControlType::CUBIC;
    engine.configureRun(original);
    control.configureRun(original);
    REQUIRE(engine.random() == control.random());

    RunConfig invalid = original;
    invalid.seed = 1234;
    SECTION("invalid congestion controller") {
        invalid.packet_size = 900;
        invalid.congestion_control = static_cast<CongestionControlType>(255);
    }
    SECTION("invalid packet size") {
        invalid.packet_size = 0;
        invalid.congestion_control = CongestionControlType::TAHOE;
    }
    REQUIRE_THROWS_AS(engine.configureRun(invalid), std::invalid_argument);
    REQUIRE(engine.getGlobalPacketSize() == 1200);
    REQUIRE(engine.random() == control.random());
    auto& session = engine.createTCPSession(0, 1);
    REQUIRE(session.getClientConnection().getCongestionControlType() == CongestionControlType::CUBIC);
    REQUIRE(session.getServerConnection().getCongestionControlType() == CongestionControlType::CUBIC);
}

TEST_CASE("RunConfig selects congestion control for new TCP sessions", "[tcp][congestion][run-config]")
{
    for (const auto type : {
            CongestionControlType::TAHOE,
            CongestionControlType::RENO,
            CongestionControlType::NEW_RENO,
            CongestionControlType::CUBIC}) {
        SimulationEngine engine(Topology(2));
        RunConfig config;
        config.congestion_control = type;
        engine.configureRun(config);

        auto& session = engine.createTCPSession(0, 1);
        REQUIRE(session.getClientConnection().getCongestionControlType() == type);
        REQUIRE(session.getServerConnection().getCongestionControlType() == type);
    }
}
