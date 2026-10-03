#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"

using namespace kns;

TEST_CASE("Engine random streams survive unrelated configuration and interleaving", "[random]")
{
    SimulationEngine alone{Topology(2)};
    SimulationEngine interleaved{Topology(2)};
    SimulationEngine other{Topology(2)};
    RunConfig config;
    config.seed = 97;
    alone.configureRun(config);
    interleaved.configureRun(config);
    config.seed = 1234;
    other.configureRun(config);
    for (int i = 0; i < 100; ++i) {
        other.random();
        other.randomSequence();
        REQUIRE(alone.random() == interleaved.random());
        REQUIRE(alone.randomSequence() == interleaved.randomSequence());
    }
}

TEST_CASE("Interleaved simulations preserve TCP sequences and packet loss", "[random][tcp]")
{
    auto make = [] {
        Topology topology(2);
        topology.addLink(0, 1, 100.0, 1.0, 0.1);
        return SimulationEngine(topology);
    };
    auto alone = make();
    auto mixed = make();
    RunConfig config;
    config.seed = 567;
    config.packet_size = 100;
    alone.configureRun(config);
    mixed.configureRun(config);
    alone.startTCPConnection(0, 1);
    mixed.startTCPConnection(0, 1);
    alone.run();
    auto other = make();
    config.seed = 890;
    other.configureRun(config);
    other.startTCPConnection(0, 1);
    while (mixed.hasEvents()) {
        other.processEvent();
        other.random();
        mixed.processEvent();
    }
    other.run();
    REQUIRE(alone.getTCPSession(0).getClientConnection().getSeqNum() ==
            mixed.getTCPSession(0).getClientConnection().getSeqNum());
    REQUIRE(alone.getStats().packets_lost == mixed.getStats().packets_lost);
    REQUIRE(alone.getStats().packets_delivered == mixed.getStats().packets_delivered);
    REQUIRE(alone.now() == mixed.now());
}
