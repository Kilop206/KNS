#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "engine/core/SimulationEngine.hpp"
#include "enums/LinkMode.hpp"
#include "network/Link.hpp"
#include "network/Packet.hpp"
#include "network/Topology.hpp"

#include "engine/events/PacketGenerationEvent.hpp"

using Catch::Approx;
using kns::Link;
using kns::LinkMode;
using kns::Packet;
using kns::PacketGenerationEvent;
using kns::SimulationEngine;
using kns::Topology;

TEST_CASE("Packet sizes are validated before transmission side effects", "[core][engine][packet-size]")
{
    Topology topology(2);
    auto link = topology.addLinkPtr(0, 1, 10.0, 1.0);
    SimulationEngine engine(topology);
    REQUIRE(engine.getGlobalPacketSize() > 0);
    engine.setGlobalPacketSize(1000);
    for (const int size : {0, -1, -1500}) {
        CAPTURE(size);
        REQUIRE_THROWS_AS(engine.setGlobalPacketSize(size), std::invalid_argument);
        REQUIRE(engine.getGlobalPacketSize() == 1000);
        Packet packet(0, 1, 0, 0.0, size, 0);
        REQUIRE_THROWS_AS(engine.sendPacket(packet, *link, 0.0), std::invalid_argument);
        REQUIRE_THROWS_AS(engine.compute_arrival_time(packet, *link, 0.0), std::invalid_argument);
        REQUIRE_FALSE(engine.hasEvents());
        REQUIRE(engine.getPacketsInTransit().empty());
        REQUIRE(link->getQueueSize() == 0);
        REQUIRE(link->getNextAvailableTime(0, 1, 0.0) == 0.0);
        REQUIRE(engine.getStats().packets_sent == 0);
    }
    for (const int size : {1, 1500, 65535}) {
        engine.setGlobalPacketSize(size);
        Packet packet(0, 1, 0, engine.now(), size, 0);
        REQUIRE(engine.sendPacket(packet, *link, engine.now()));
        const auto& travel = engine.getPacketsInTransit().back();
        REQUIRE(travel.arrival_time >= travel.departure_time);
        engine.run();
    }
}

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

TEST_CASE("SimulationEngine lifecycle state transitions", "[core][engine][state]")
{
    Topology topo(2);
    topo.addLink(0, 1, 100.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);
    SimulationEngine engine(topo);

    REQUIRE_FALSE(engine.hasEvents());
    
    auto& sess = engine.createTCPSession(0, 1);
    engine.schedule(std::make_unique<PacketGenerationEvent>(engine.now(), 0, 1, sess.getSession_id()));
    REQUIRE(engine.hasEvents());

    engine.run();
    REQUIRE_FALSE(engine.hasEvents());
}
