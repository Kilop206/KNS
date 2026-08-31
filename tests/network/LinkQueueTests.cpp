#include <catch2/catch_test_macros.hpp>
#include "network/Topology.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"
#include "network/utils/PacketUtils.hpp"
#include "enums/LinkMode.hpp"

using namespace kns;

TEST_CASE("Link queue produces expected serialization of transmissions", "[network][link][queue]") {
    Topology topo(2);
    // create link of 1 Mbps, 0 ms prop delay to make tx time visible
    topo.addLink(0, 1, 1.0, 0.0, 0.0, LinkMode::FULL_DUPLEX);

    SimulationEngine engine(topo);
    std::vector<double> arrivals;

    engine.setPacketObserver([&](const Packet& p, uint64_t, int from, int to, double dep, double arr) {
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