#include <catch2/catch_test_macros.hpp>
#include "network/Topology.hpp"

using namespace kns;

TEST_CASE("Topology synchronization preserves RED for new and retained links", "[network][discovery][red]")
{
    Topology snapshot(2);
    auto incoming = snapshot.addLinkPtr(0, 1, 100.0, 1.0);
    incoming->configureRed(2, 8, 0.5);
    Topology topology(2);
    REQUIRE(topology.synchronizeFrom(snapshot));
    auto link = topology.getLinks().front();
    const auto id = link->getId();
    REQUIRE(link->getQueueDiscipline() == Link::QueueDiscipline::RED);
    REQUIRE(link->getRedMaxThreshold() == 8);
    REQUIRE_FALSE(topology.synchronizeFrom(snapshot));

    // Editing queue policy must retain link identity and pending arrivals.
    link->enqueueTransmission(0, 1, 0.0, 1.0);
    incoming->configureRed(4, 12, 0.25);
    REQUIRE(topology.synchronizeFrom(snapshot));
    REQUIRE(link->getId() == id);
    REQUIRE(link->getQueueSize() == 1);
    REQUIRE(link->getRedMinThreshold() == 4);
    REQUIRE(link->getRedMaxThreshold() == 12);
    REQUIRE(link->getRedMaxDropProbability() == 0.25);
    REQUIRE_FALSE(topology.synchronizeFrom(snapshot));

    incoming->setDropTail();
    REQUIRE(topology.synchronizeFrom(snapshot));
    REQUIRE(link->getQueueDiscipline() == Link::QueueDiscipline::DROP_TAIL);
    REQUIRE(link->dequeueTransmission(0, 1, 0.0, 1.0));
    REQUIRE_FALSE(topology.synchronizeFrom(snapshot));
}

TEST_CASE("Topology synchronization reports loss-only edits", "[network][discovery]")
{
    Topology snapshot(2);
    auto incoming = snapshot.addLinkPtr(0, 1, 100.0, 1.0);
    Topology topology(2);
    topology.synchronizeFrom(snapshot);
    incoming->setLossProb(0.5);
    REQUIRE(topology.synchronizeFrom(snapshot));
    REQUIRE(topology.getLinks().front()->getLossProb() == 0.5);
    REQUIRE_FALSE(topology.synchronizeFrom(snapshot));
}
