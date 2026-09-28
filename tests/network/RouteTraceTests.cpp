#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/core/SimulationEngine.hpp"

using namespace kns;

TEST_CASE("Route inspection follows exact forwarding links for every metric", "[network][route-trace]") {
    for (auto metric:{RoutingMetric::Delay,RoutingMetric::Bandwidth,
            RoutingMetric::HopCount,RoutingMetric::DelayBandwidth}) {
        CAPTURE(static_cast<int>(metric));
        Topology topology(3);
        auto slow=topology.addLinkPtr(0,1,10,20);
        auto fast=topology.addLinkPtr(0,1,100,2);
        auto last=topology.addLinkPtr(1,2,50,3);
        SimulationEngine engine(topology);
        engine.setRoutingMetric(metric);
        const auto revision=engine.getTopology().getRoutingRevision();
        const auto trace=engine.traceRoute(0,2);
        REQUIRE(trace.status==RouteStatus::Reachable);
        REQUIRE(trace.hops.size()==2);
        const auto chosen=metric==RoutingMetric::HopCount ? slow : fast;
        REQUIRE(trace.hops[0].link_id==chosen->getId());
        REQUIRE(trace.hops[1].link_id==last->getId());
        for (const auto& hop:trace.hops) {
            REQUIRE(hop.to==engine.getNextHop(hop.from,2));
            REQUIRE(hop.link_id==engine.getRoutingTable(hop.from)[2].link_id);
        }
        REQUIRE(trace.propagation_delay_ms==Catch::Approx(chosen->getDelayMs()+3));
        REQUIRE(trace.bottleneck_mbps==std::optional<double>{metric==RoutingMetric::HopCount ? 10.0 : 50.0});
        REQUIRE(engine.now()==0);
        REQUIRE_FALSE(engine.hasEvents());
        REQUIRE(engine.getTCPSessions().empty());
        REQUIRE(engine.getPacketsInTransit().empty());
        REQUIRE(engine.getTopology().getRoutingRevision()==revision);
    }
}

TEST_CASE("Route inspection refreshes after metric and live topology changes", "[network][route-trace]") {
    Topology topology(3);
    topology.addLink(0,2,10,1);
    topology.addLink(0,1,100,2);
    topology.addLink(1,2,100,2);
    SimulationEngine engine(topology);
    REQUIRE(engine.traceRoute(0,2).hops.size()==1);
    engine.setRoutingMetric(RoutingMetric::Bandwidth);
    REQUIRE(engine.traceRoute(0,2).hops.size()==2);
    auto& live=engine.getTopology();
    live.getLinks()[2]->setUp(false);
    REQUIRE(engine.traceRoute(0,2).hops.size()==1);
    live.getLinks()[0]->setUp(false);
    REQUIRE(engine.traceRoute(0,2).status==RouteStatus::Unreachable);
    live.getLinks()[2]->setUp(true);
    REQUIRE(engine.traceRoute(0,2).hops.size()==2);
    REQUIRE(live.removeNode(1));
    REQUIRE(engine.traceRoute(0,2).status==RouteStatus::Unreachable);
    REQUIRE(engine.traceRoute(0,1).status==RouteStatus::InvalidEndpoint);
}

TEST_CASE("Route inspection handles direction invalid endpoints and self routes", "[network][route-trace]") {
    Topology topology(3);
    topology.addLink(0,1,100,1,0,LinkMode::SIMPLEX);
    SimulationEngine engine(topology);
    REQUIRE(engine.traceRoute(0,1).status==RouteStatus::Reachable);
    REQUIRE(engine.traceRoute(1,0).status==RouteStatus::Unreachable);
    REQUIRE(engine.traceRoute(0,2).status==RouteStatus::Unreachable);
    for (auto pair:{std::pair{-1,0},std::pair{0,3},std::pair{3,3}}) {
        const auto trace=engine.traceRoute(pair.first,pair.second);
        REQUIRE(trace.status==RouteStatus::InvalidEndpoint);
        REQUIRE(trace.hops.empty());
        REQUIRE_FALSE(trace.bottleneck_mbps);
    }
    const auto self=engine.traceRoute(0,0);
    REQUIRE(self.status==RouteStatus::Reachable);
    REQUIRE(self.hops.empty());
    REQUIRE(self.propagation_delay_ms==0);
    REQUIRE_FALSE(self.bottleneck_mbps);
    REQUIRE(engine.deleteNode(0));
    REQUIRE(engine.traceRoute(0,0).status==RouteStatus::InvalidEndpoint);
}
