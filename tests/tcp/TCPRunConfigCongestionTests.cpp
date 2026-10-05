#include <catch2/catch_test_macros.hpp>

#include "engine/core/RunConfig.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/Topology.hpp"

using namespace kns;

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
