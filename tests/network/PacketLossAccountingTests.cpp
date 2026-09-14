#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "network/utils/PacketUtils.hpp"

using namespace kns;

TEST_CASE("Topology rejection counts one loss at source and intermediate hops", "[network][loss][stats]")
{
    for (const bool intermediate : {false, true}) {
        for (const int failure : {0, 1, 2, 3}) {
            CAPTURE(intermediate, failure);
            Topology topology(3);
            topology.addLinkPtr(0, 1, 100.0, 1.0);
            auto target = topology.addLinkPtr(1, 2, 100.0, 1000.0, 0.0, LinkMode::FULL_DUPLEX, 1);
            SimulationEngine engine(topology);
            Packet packet(intermediate ? 0 : 1, 2, intermediate ? 0 : 1, 0.0, 100, 999);
            if (intermediate) {
                REQUIRE(PacketUtils::sendPacketThroughTopology(engine, packet));
            }

            if (failure == 0) {
                target->setLossProb(1.0);
            } else if (failure == 1) {
                Packet blocker(1, 2, 1, 0.0, 100, 999);
                REQUIRE(engine.sendPacket(blocker, *target, 0.0));
                REQUIRE_FALSE(target->canQueue(1, 2));
            } else if (failure == 2) {
                target->setUp(false);
            } else {
                REQUIRE(engine.getTopology().removeLinkById(target->getId()));
            }
            const int before = engine.getStats().packets_lost;
            if (intermediate) {
                REQUIRE(engine.processEvent());
            } else {
                REQUIRE_FALSE(PacketUtils::sendPacketThroughTopology(engine, packet));
            }
            REQUIRE(engine.getStats().packets_lost == before + 1);
            engine.run();
            REQUIRE(engine.getStats().packets_lost == before + 1);
            REQUIRE(engine.getPacketsInTransit().empty());
        }
    }
}
