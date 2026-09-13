#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <stdexcept>
#include "network/Topology.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"
#include "network/utils/PacketUtils.hpp"
#include "enums/LinkMode.hpp"

using namespace kns;

TEST_CASE("Propagation does not reserve the transmitter", "[network][link][timing]")
{
    for (const auto mode : {LinkMode::FULL_DUPLEX, LinkMode::HALF_DUPLEX, LinkMode::SIMPLEX}) {
        Topology topology(2);
        auto link = topology.addLinkPtr(0, 1, 1.0, 1000.0, 0.0, mode, 2);
        SimulationEngine engine(topology);
        Packet packet(0, 1, 0, 0.0, 125, 999);
        REQUIRE(engine.sendPacket(packet, *link, 0.0));
        REQUIRE(engine.sendPacket(packet, *link, 0.0));
        const auto& travel = engine.getPacketsInTransit();
        REQUIRE(travel.size() == 2);
        REQUIRE(link->getNextAvailableTime(0, 1, 0.0) == Catch::Approx(0.002));
        REQUIRE_FALSE(link->canQueue(0, 1));
        if (mode == LinkMode::FULL_DUPLEX) {
            REQUIRE(link->getNextAvailableTime(1, 0, 0.0) == 0.0);
        } else if (mode == LinkMode::HALF_DUPLEX) {
            REQUIRE(link->getNextAvailableTime(1, 0, 0.0) == Catch::Approx(0.002));
        } else {
            REQUIRE_FALSE(link->allowsTransmission(1, 0));
        }
        REQUIRE(engine.peekNextEventTime() == Catch::Approx(1.001));
        engine.run();
        REQUIRE(engine.now() == Catch::Approx(1.002));
        REQUIRE(link->getQueueSize() == 0);
    }
}

TEST_CASE("Sending rejects foreign and removed links without side effects", "[network][link][ownership]")
{
    Topology topology(2);
    auto owned = topology.addLinkPtr(0, 1, 10.0, 1.0);
    SimulationEngine engine(topology);
    Link foreign(0, 1, 10.0, 1.0);
    Link copied = *owned;
    REQUIRE(engine.getTopology().removeLinkById(owned->getId()));
    Packet packet(0, 1, 0, 0.0, 100, 999);
    for (auto* link : {&foreign, &copied, owned.get()}) {
        REQUIRE_FALSE(engine.sendPacket(packet, *link, 0.0));
        REQUIRE(link->getQueueSize() == 0);
        REQUIRE(link->getNextAvailableTime(0, 1, 0.0) == 0.0);
        REQUIRE(engine.getStats().packets_sent == 0);
        REQUIRE(engine.getStats().packets_lost == 0);
        REQUIRE(engine.getPacketsInTransit().empty());
        REQUIRE_FALSE(engine.hasEvents());
    }
}

TEST_CASE("Configured queues reject overflow and drain through arrival events", "[network][link][queue][integration]")
{
    for (const auto mode : {LinkMode::FULL_DUPLEX, LinkMode::HALF_DUPLEX}) {
        for (const int capacity : {1, 3}) {
            CAPTURE(mode, capacity);
            Topology topology(2);
            auto link = topology.addLinkPtr(0, 1, 10.0, 1.0, 0.0, mode, capacity);
            SimulationEngine engine(topology);
            Packet packet(0, 1, 0, 0.0, 100, 999);
            for (int i = 0; i < capacity; ++i) {
                REQUIRE(engine.sendPacket(packet, *link, 0.0));
            }
            REQUIRE_FALSE(engine.sendPacket(packet, *link, 0.0));
            REQUIRE(engine.getStats().packets_lost == 1);
            const auto next_mode = mode == LinkMode::FULL_DUPLEX
                ? LinkMode::HALF_DUPLEX : LinkMode::SIMPLEX;
            REQUIRE_THROWS_AS(link->setMode(next_mode), std::logic_error);
            Packet reverse(1, 0, 1, 0.0, 100, 999);
            REQUIRE(engine.sendPacket(reverse, *link, 0.0) == (mode == LinkMode::FULL_DUPLEX));
            engine.run();
            REQUIRE(link->getQueueSize() == 0);
            REQUIRE(engine.getPacketsInTransit().empty());
            REQUIRE_NOTHROW(link->setMode(next_mode));
            REQUIRE(engine.sendPacket(packet, *link, engine.now()));
            engine.run();
            REQUIRE(link->getQueueSize() == 0);
        }
    }
}

TEST_CASE("Link queue produces expected serialization of transmissions", "[network][link][queue]") {
    Topology topo(2);
    // create link of 1 Mbps, 0 ms prop delay to make tx time visible
    topo.addLink(0, 1, 1.0, 0.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topo);
    std::vector<double> arrivals;

    engine.setPacketObserver([&](const Packet& /* p */, uint64_t, int from, int to, double /* dep */, double arr) {
        if (from == 0 && to == 1) arrivals.push_back(arr);
    });

    // craft two packets with size 125000 bytes -> transmission_time = 1.0s at 1 Mbps
    Packet p1(0,1,0, engine.now(), 125000, 1);
    Packet p2(0,1,0, engine.now(), 125000, 2);

    PacketUtils::sendPacketThroughTopology(engine, p1);
    PacketUtils::sendPacketThroughTopology(engine, p2);

    engine.run();

    REQUIRE(arrivals.size() == 2);
    const double eps = 1e-6;
    REQUIRE(arrivals[1] - arrivals[0] >= 1.0 - eps);
}

TEST_CASE("PacketUtils sendPacketThroughTopology propagates drop status", "[network][utils]")
{
    Topology topo(2);
    // Link with 100% loss probability
    topo.addLink(0, 1, 100.0, 5.0, 1.0, LinkMode::FULL_DUPLEX);
    SimulationEngine engine(topo);
    auto& sess = engine.createTCPSession(0, 1);

    Packet p(0, 1, 0, engine.now(), 1000, sess.getSession_id());

    // Should return false when the packet is dropped by the link
    bool result = PacketUtils::sendPacketThroughTopology(engine, p);
    REQUIRE_FALSE(result);
}

TEST_CASE(
    "Link rejects transmission when queue capacity is full",
    "[network][link][queue]"
) {
    Link link(0, 1, 100.0, 1.0, 0.0);

    for (std::size_t i = 0; i < link.getQueueCapacity(); ++i) {
        REQUIRE(link.canQueue(0, 1));

        link.enqueueTransmission(
            0,
            1,
            static_cast<double>(i),
            static_cast<double>(i + 1)
        );
    }

    REQUIRE(link.getQueueSize() == link.getQueueCapacity());
    REQUIRE_FALSE(link.canQueue(0, 1));
}

TEST_CASE(
    "FULL_DUPLEX queues are independent",
    "[network][link][queue]"
) {
    Link link(
        0,
        1,
        100.0,
        1.0,
        0.0,
        LinkMode::FULL_DUPLEX
    );

    link.enqueueTransmission(0, 1, 0.0, 1.0);

    REQUIRE(link.estimatedQueueSize(0.0, 0, 1) == 1);
    REQUIRE(link.estimatedQueueSize(0.0, 1, 0) == 0);

    link.enqueueTransmission(1, 0, 0.0, 1.0);

    REQUIRE(link.estimatedQueueSize(0.0, 0, 1) == 1);
    REQUIRE(link.estimatedQueueSize(0.0, 1, 0) == 1);
}

TEST_CASE(
    "HALF_DUPLEX shares one queue",
    "[network][link][queue]"
) {
    Link link(
        0,
        1,
        100.0,
        1.0,
        0.0,
        LinkMode::HALF_DUPLEX
    );

    link.enqueueTransmission(0, 1, 0.0, 1.0);

    REQUIRE(link.estimatedQueueSize(0.0, 0, 1) == 1);
    REQUIRE(link.estimatedQueueSize(0.0, 1, 0) == 1);

    REQUIRE(link.canQueue(1, 0));
}

TEST_CASE(
    "SIMPLEX rejects reverse direction",
    "[network][link][queue]"
) {
    Link link(
        0,
        1,
        100.0,
        1.0,
        0.0,
        LinkMode::SIMPLEX
    );

    REQUIRE(link.canQueue(0, 1));
    REQUIRE_FALSE(link.canQueue(1, 0));
}
