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
