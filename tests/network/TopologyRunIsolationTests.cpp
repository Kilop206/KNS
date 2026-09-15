#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"

using namespace kns;

TEST_CASE("New runs preserve configuration but never inherit link runtime", "[topology][restart]")
{
    Topology source(3);
    source.setName("restart");
    source.setNodeLabel(0, "source");
    source.removeNode(2);
    auto original = source.addLinkPtr(0, 1, 1.0, 1000.0, 0.0, LinkMode::HALF_DUPLEX, 2);
    auto second = source.addLinkPtr(0, 1, 2.0, 5.0, 0.2);
    second->setUp(false);
    SimulationEngine first(source);
    auto active = first.getTopology().getLinks()[0];
    Packet packet(0, 1, 0, 0.0, 1000, 999);
    REQUIRE(first.sendPacket(packet, *active, 0.0));
    REQUIRE(active->getQueueSize() == 1);
    REQUIRE(original->getQueueSize() == 0);
    for (bool completed : {false, true}) {
        if (completed) first.run();
        SimulationEngine restarted(first.getTopology());
        auto fresh = restarted.getTopology().getLinks()[0];
        REQUIRE(fresh.get() != active.get());
        REQUIRE(fresh->getId() == original->getId());
        REQUIRE(fresh->getQueueSize() == 0);
        REQUIRE(fresh->getNextAvailableTime(0, 1, 0.0) == 0.0);
        REQUIRE(restarted.getTopology().getInterfaces().size() == source.getInterfaces().size());
        REQUIRE_FALSE(restarted.getTopology().getNode(2)->isActive());
        REQUIRE_FALSE(restarted.getTopology().getLinks()[1]->isUp());
        REQUIRE(restarted.getTopology().getLinks()[1]->getLossProb() == 0.2);
        fresh->setUp(false);
        REQUIRE(active->isUp());
        REQUIRE(original->isUp());
        fresh->setUp(true);
        REQUIRE(restarted.sendPacket(packet, *fresh, 0.0));
        REQUIRE(restarted.getPacketsInTransit()[0].departure_time == 0.0);
    }
}
