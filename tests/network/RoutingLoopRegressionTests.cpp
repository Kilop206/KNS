#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/PacketReceivedEvent.hpp"
#include <random>

using namespace kns;

TEST_CASE("Equal cost routes make forwarding progress on small connected graphs", "[routing][audit]") {
    std::mt19937 random(731);
    for (int sample = 0; sample < 120; ++sample) {
        Topology topology(5);
        for (int a = 0; a < 5; ++a) {
            for (int b = a + 1; b < 5; ++b) {
                if (b == a + 1 || random() % 2)
                    topology.addLink(a, b, 1 + random() % 3, random() % 2, 0);
            }
        }
        SimulationEngine engine(topology);
        for (auto metric : {RoutingMetric::Delay, RoutingMetric::Bandwidth, RoutingMetric::DelayBandwidth}) {
            engine.setRoutingMetric(metric);
            for (int a = 0; a < 5; ++a) for (int b = 0; b < 5; ++b) {
                CAPTURE(sample, a, b, static_cast<int>(metric));
                REQUIRE(engine.traceRoute(a, b).status == RouteStatus::Reachable);
            }
        }
    }
}

TEST_CASE("TCP traffic has a finite hop budget even during forwarding loops", "[routing][audit]") {
    Topology topology(3);
    topology.addLink(0, 1, 100, 1, 0);
    topology.addLink(1, 2, 100, 1, 0);
    SimulationEngine engine(topology);
    Packet packet(0, 2, 1, 0, 100, 0);
    packet.hop_count = 4096;
    PacketReceivedEvent event(0, packet);
    event.execute(engine);
    REQUIRE_FALSE(engine.hasEvents());
    REQUIRE(engine.getStats().packets_lost == 1);
}
