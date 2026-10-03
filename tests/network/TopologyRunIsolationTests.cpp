#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"

using namespace kns;

TEST_CASE("Loss override is explicit reversible and preserves topology configuration", "[loss][configuration]")
{
    Topology source(2);
    source.addLink(0, 1, 100.0, 1.0, 1.0);
    source.addLink(0, 1, 100.0, 1.0, 0.2);
    for (int run = 0; run < 2; ++run) {
        SimulationEngine engine(source);
        auto link = engine.getTopology().getLinks()[0];
        Packet packet(0, 1, 0, 0.0, 100, 999);
        REQUIRE_FALSE(engine.hasGlobalLossOverride());
        REQUIRE_FALSE(engine.sendPacket(packet, *link, 0.0));
        engine.setGlobalLossProb(0.0f);
        REQUIRE(engine.hasGlobalLossOverride());
        REQUIRE(engine.sendPacket(packet, *link, 0.0));
        engine.run();
        REQUIRE(link->getLossProb() == 1.0);
        REQUIRE(engine.getTopology().getLinks()[1]->getLossProb() == 0.2);
        engine.clearGlobalLossOverride();
        REQUIRE_FALSE(engine.sendPacket(packet, *link, engine.now()));
        REQUIRE_THROWS_AS(engine.setGlobalLossProb(-1.0f), std::invalid_argument);
        REQUIRE_FALSE(engine.hasGlobalLossOverride());
        REQUIRE(source.getLinks()[0]->getLossProb() == 1.0);
    }
}

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
