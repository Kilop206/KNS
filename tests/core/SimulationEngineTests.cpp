#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "engine/core/SimulationEngine.hpp"
#include "enums/LinkMode.hpp"
#include "network/Link.hpp"
#include "network/Packet.hpp"
#include "network/Topology.hpp"

using Catch::Approx;
using kns::Link;
using kns::LinkMode;
using kns::Packet;
using kns::SimulationEngine;
using kns::Topology;

TEST_CASE("SimulationEngine computes arrival time from propagation and transmission delay", "[core][engine]")
{
    Topology topo(2);
    topo.addLink(0, 1, 100.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);
    SimulationEngine engine(topo);

    Link link(0, 1, 10.0, 25.0, 0.0, LinkMode::FULL_DUPLEX);
    Packet packet(0, 1, 0, 0.0, 1500, 1);

    const double arrival = engine.compute_arrival_time(packet, link, 2.0);

    // 25 ms propagation + 1500 bytes * 8 / 10 Mbps = 0.025 + 0.0012 seconds.
    REQUIRE(arrival == Approx(2.0262));
}
