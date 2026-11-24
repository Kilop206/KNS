#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "analysis/RouteResilience.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/PrintEvent.hpp"
#include <stdexcept>

using namespace kns;
using kns::analysis::RouteResilience;

namespace {
    void checkSameRoute(const RouteTrace& actual, const RouteTrace& expected) {
        REQUIRE(actual.status == expected.status);
        REQUIRE(actual.hops.size() == expected.hops.size());
        REQUIRE(actual.propagation_delay_ms == Catch::Approx(expected.propagation_delay_ms));
        REQUIRE(actual.bottleneck_mbps == expected.bottleneck_mbps);
        for (std::size_t i = 0; i < actual.hops.size(); ++i) {
            REQUIRE(actual.hops[i].from == expected.hops[i].from);
            REQUIRE(actual.hops[i].to == expected.hops[i].to);
            REQUIRE(actual.hops[i].link_id == expected.hops[i].link_id);
        }
    }
}

TEST_CASE("Cable outage previews match actual converged forwarding for every metric",
          "[network][resilience]") {
    for (auto metric : {RoutingMetric::Delay, RoutingMetric::Bandwidth,
                        RoutingMetric::HopCount, RoutingMetric::DelayBandwidth}) {
        CAPTURE(static_cast<int>(metric));
        Topology topology(5);
        topology.addLink(0, 1, 100, 0);
        topology.addLink(0, 1, 10, 1); // Parallel cable must survive the first one's outage.
        topology.addLink(1, 4, 100, 1);
        topology.addLink(0, 2, 50, 3, 0, LinkMode::SIMPLEX);
        topology.addLink(2, 3, 50, 3);
        topology.addLink(3, 4, 50, 3);
        SimulationEngine engine(topology);
        engine.setRoutingMetric(metric);
        RouteResilience analysis(engine, 0, 4);
        while (analysis.advance(engine)) {}
        REQUIRE(analysis.complete());
        REQUIRE(analysis.criticalCount() == 0);
        for (const auto& failure : analysis.failures()) {
            auto changed = topology.cloneForRun();
            REQUIRE(changed.setLinkUpById(failure.failed_hop.link_id, false));
            SimulationEngine actual(changed);
            actual.setRoutingMetric(metric);
            checkSameRoute(failure.alternative, actual.traceRoute(0, 4));
            for (const auto& hop : failure.alternative.hops)
                REQUIRE(hop.link_id != failure.failed_hop.link_id);
        }
    }
}

TEST_CASE("Resilience distinguishes critical cables from slower lower capacity alternatives",
          "[network][resilience]") {
    Topology topology(5);
    auto critical = topology.addLinkPtr(0, 1, 100, 1);
    auto primary = topology.addLinkPtr(1, 4, 100, 1);
    topology.addLink(1, 2, 20, 3);
    topology.addLink(2, 4, 10, 4);
    SimulationEngine engine(topology);
    RouteResilience analysis(engine, 0, 4);
    REQUIRE_FALSE(analysis.finished());
    REQUIRE(analysis.advance(engine));
    REQUIRE(analysis.failures().size() == 1);
    REQUIRE(analysis.failures()[0].failed_hop.link_id == critical->getId());
    REQUIRE(analysis.failures()[0].alternative.status == RouteStatus::Unreachable);
    REQUIRE_FALSE(analysis.complete());
    REQUIRE(analysis.advance(engine));
    REQUIRE(analysis.complete());
    REQUIRE(analysis.finished());
    REQUIRE_FALSE(analysis.advance(engine));
    REQUIRE(analysis.criticalCount() == 1);
    const auto& rerouted = analysis.failures()[1];
    REQUIRE(rerouted.failed_hop.link_id == primary->getId());
    REQUIRE(rerouted.alternative.status == RouteStatus::Reachable);
    REQUIRE(rerouted.alternative.propagation_delay_ms == Catch::Approx(8));
    REQUIRE(rerouted.alternative.bottleneck_mbps == std::optional{10.0});
}

TEST_CASE("Outage alternatives respect direction active nodes and device forwarding roles",
          "[network][resilience]") {
    Topology topology(3);
    auto direct = topology.addLinkPtr(0, 2, 100, 1);
    topology.addLink(0, 1, 100, 2);
    topology.addLink(2, 1, 100, 2, 0, LinkMode::SIMPLEX);
    SimulationEngine engine(topology);
    REQUIRE(engine.traceRoute(0, 2, direct->getId()).status == RouteStatus::Unreachable);
    REQUIRE(engine.traceRoute(2, 0, direct->getId()).status == RouteStatus::Reachable);
    DeviceInfo endpoint;
    endpoint.type = DeviceType::Computer;
    REQUIRE(engine.getTopology().setNodeDeviceInfo(1, endpoint));
    REQUIRE(engine.traceRoute(2, 0, direct->getId()).status == RouteStatus::Unreachable);
    REQUIRE(engine.getTopology().removeNode(1));
    REQUIRE(engine.traceRoute(2, 0, direct->getId()).status == RouteStatus::Unreachable);
    REQUIRE(engine.traceRoute(-1, 0, direct->getId()).status == RouteStatus::InvalidEndpoint);
}

TEST_CASE("Bounded resilience never presents truncated or unreachable analysis as complete",
          "[network][resilience]") {
    Topology topology(4);
    topology.addLink(0, 1, 100, 1);
    topology.addLink(1, 2, 100, 1);
    SimulationEngine engine(topology);
    RouteResilience bounded(engine, 0, 2, 1);
    REQUIRE(bounded.advance(engine));
    REQUIRE(bounded.finished());
    REQUIRE_FALSE(bounded.complete());
    REQUIRE_FALSE(bounded.advance(engine));
    REQUIRE(bounded.failures().size() == 1);
    for (auto pair : {std::pair{0, 3}, std::pair{-1, 2}}) {
        RouteResilience unavailable(engine, pair.first, pair.second);
        REQUIRE(unavailable.finished());
        REQUIRE_FALSE(unavailable.complete());
        REQUIRE_FALSE(unavailable.advance(engine));
        REQUIRE(unavailable.failures().empty());
    }
    RouteResilience local(engine, 0, 0);
    REQUIRE(local.finished());
    REQUIRE(local.complete());
    REQUIRE(local.failures().empty());
    REQUIRE_THROWS_AS(RouteResilience(engine, 0, 2, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(RouteResilience(engine, 0, 2, 257), std::invalid_argument);
}

TEST_CASE("Resilience rejects stale inputs and unrelated simulation engines",
          "[network][resilience]") {
    Topology topology(3);
    topology.addLink(0, 1, 100, 1);
    topology.addLink(1, 2, 100, 1);
    SimulationEngine engine(topology), other(topology);
    RouteResilience analysis(engine, 0, 2);
    REQUIRE_FALSE(analysis.matches(other, 0, 2));
    REQUIRE_FALSE(analysis.matches(engine, 2, 0));
    REQUIRE_THROWS_AS(analysis.advance(other), std::logic_error);
    SECTION("metric") { engine.setRoutingMetric(RoutingMetric::Bandwidth); }
    SECTION("link") { engine.getTopology().getLinks()[0]->setUp(false); }
    SECTION("role") {
        DeviceInfo endpoint;
        endpoint.type = DeviceType::Server;
        engine.getTopology().setNodeDeviceInfo(1, endpoint);
    }
    REQUIRE_FALSE(analysis.matches(engine, 0, 2));
    REQUIRE_THROWS_AS(analysis.advance(engine), std::logic_error);
}

TEST_CASE("Outage previews preserve pending transmissions scheduler counters and random streams",
          "[network][resilience]") {
    Topology topology(3);
    topology.addLink(0, 1, 100, 1, 0.3);
    topology.addLink(1, 2, 100, 1);
    SimulationEngine engine(topology), control(topology);
    auto link = engine.getTopology().getLinks()[0];
    link->enqueueTransmission(0, 1, 0, 1);
    link->reserveTransmission(0, 1, 1);
    engine.schedule(std::make_unique<PrintEvent>(2, "pending"));
    const auto revision = engine.getTopology().getRoutingRevision();
    const auto events = engine.pendingEventCount();
    const auto counters = engine.executionStatistics();
    RouteResilience analysis(engine, 0, 2);
    while (analysis.advance(engine)) {}
    REQUIRE(engine.getTopology().getRoutingRevision() == revision);
    REQUIRE(engine.pendingEventCount() == events);
    REQUIRE(engine.executionStatistics().events_dispatched == counters.events_dispatched);
    REQUIRE(engine.now() == 0);
    REQUIRE(link->isUp());
    REQUIRE(link->getQueueSize() == 1);
    REQUIRE(link->getNextAvailableTime(0, 1, 0) == 1);
    REQUIRE(engine.getStats().packets_sent == 0);
    REQUIRE(engine.getStats().packets_lost == 0);
    for (int i = 0; i < 20; ++i) {
        REQUIRE(engine.randomSequence() == control.randomSequence());
        REQUIRE(link->should_drop() == control.getTopology().getLinks()[0]->should_drop());
    }
}
