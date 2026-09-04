#include <catch2/catch_test_macros.hpp>
#include "network/Link.hpp"
#include "network/Topology.hpp"
#include "enums/LinkMode.hpp"
#include <limits>

using kns::Link;
using kns::LinkMode;
using kns::Topology;

TEST_CASE("Link construction and basic attributes", "[network][link]")
{
    Link link(1, 2, 100.0, 10.0, 0.05, LinkMode::FULL_DUPLEX);

    REQUIRE(link.getA() == 1);
    REQUIRE(link.getB() == 2);
    REQUIRE(link.getOtherNode(1) == 2);
    REQUIRE(link.getOtherNode(2) == 1);
    REQUIRE(link.getOtherNode(3) == -1);

    REQUIRE(link.getBandwidthMbps() == 100.0);
    REQUIRE(link.getDelayMs() == 10.0);
    REQUIRE(link.getLossProb() == 0.05);
    REQUIRE(link.getMode() == LinkMode::FULL_DUPLEX);

    link.setBandwidthMbps(50.0);
    REQUIRE(link.getBandwidthMbps() == 50.0);

    link.setDelayMs(20.0);
    REQUIRE(link.getDelayMs() == 20.0);

    link.setLossProb(0.1);
    REQUIRE(link.getLossProb() == 0.1);

    link.setMode(LinkMode::HALF_DUPLEX);
    REQUIRE(link.getMode() == LinkMode::HALF_DUPLEX);
}

TEST_CASE("Link full-duplex transmission model", "[network][link]")
{
    Link link(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);

    // Initial state: not busy
    REQUIRE_FALSE(link.isBusy(1, 2, 0.0));
    REQUIRE_FALSE(link.isBusy(2, 1, 0.0));

    // Reserve A->B
    link.reserveTransmission(1, 2, 15.0);
    REQUIRE(link.isBusy(1, 2, 10.0));
    REQUIRE_FALSE(link.isBusy(1, 2, 20.0));

    // Full duplex: B->A should remain idle/not busy
    REQUIRE_FALSE(link.isBusy(2, 1, 10.0));
}

TEST_CASE("Link half-duplex transmission model", "[network][link]")
{
    Link link(1, 2, 10.0, 5.0, 0.0, LinkMode::HALF_DUPLEX);

    REQUIRE_FALSE(link.isBusy(1, 2, 0.0));
    REQUIRE_FALSE(link.isBusy(2, 1, 0.0));

    // Reserve A->B
    link.reserveTransmission(1, 2, 15.0);

    // Half duplex: both directions are busy until 15.0
    REQUIRE(link.isBusy(1, 2, 10.0));
    REQUIRE(link.isBusy(2, 1, 10.0));

    REQUIRE_FALSE(link.isBusy(1, 2, 20.0));
    REQUIRE_FALSE(link.isBusy(2, 1, 20.0));
}

TEST_CASE("Link simplex transmission model", "[network][link]")
{
    Link link(1, 2, 10.0, 5.0, 0.0, LinkMode::SIMPLEX);

    // A->B is allowed
    REQUIRE_FALSE(link.isBusy(1, 2, 0.0));

    // B->A is NOT allowed and should return busy (infinity)
    REQUIRE(link.isBusy(2, 1, 0.0));
    REQUIRE(link.getNextAvailableTime(2, 1, 0.0) == std::numeric_limits<double>::infinity());

    // Reserve A->B
    link.reserveTransmission(1, 2, 15.0);
    REQUIRE(link.isBusy(1, 2, 10.0));
}

TEST_CASE("Link queue management limits", "[network][link][queue]")
{
    Link link(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);

    REQUIRE(link.getQueueSize() == 0);
    REQUIRE(link.getQueueCapacity() == 32);
    REQUIRE(link.canQueue(1, 2));

    for (std::size_t i = 0; i < link.getQueueCapacity(); ++i) {
        REQUIRE(link.canQueue(1, 2));

        link.enqueueTransmission(
            1,
            2,
            static_cast<double>(i),
            static_cast<double>(i + 1)
        );
    }

    REQUIRE(link.getQueueSize() == link.getQueueCapacity());
    REQUIRE_FALSE(link.canQueue(1, 2));

    REQUIRE(
        link.dequeueTransmission(
            1,
            2,
            0.0,
            1.0
        )
    );

    REQUIRE(link.getQueueSize() == 31);
    REQUIRE(link.canQueue(1, 2));
}

TEST_CASE("Link drop behavior based on probability", "[network][link]")
{
    Link link_no_loss(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    REQUIRE_FALSE(link_no_loss.should_drop());

    Link link_total_loss(1, 2, 10.0, 5.0, 1.0, LinkMode::FULL_DUPLEX);
    REQUIRE(link_total_loss.should_drop());
}

TEST_CASE("Topology getLinksFromNode bounds safety and const consistency", "[network][topology]")
{
    Topology topo(2);
    topo.addLink(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    const auto& const_topo = topo;

    // Valid node: both const and non-const overloads return identical link list
    REQUIRE(topo.getLinksFromNode(0).size() == 1);
    REQUIRE(const_topo.getLinksFromNode(0).size() == 1);
    REQUIRE(topo.getLinksFromNode(0)[0]->getId() == const_topo.getLinksFromNode(0)[0]->getId());
    REQUIRE(topo.getLinksFromNode(1).size() == 1);
    REQUIRE(const_topo.getLinksFromNode(1).size() == 1);

    // Invalid negative node ID throws std::out_of_range consistently
    REQUIRE_THROWS_AS(topo.getLinksFromNode(-1), std::out_of_range);
    REQUIRE_THROWS_AS(const_topo.getLinksFromNode(-1), std::out_of_range);

    // Invalid node ID above range throws std::out_of_range consistently
    REQUIRE_THROWS_AS(topo.getLinksFromNode(2), std::out_of_range);
    REQUIRE_THROWS_AS(const_topo.getLinksFromNode(2), std::out_of_range);
    REQUIRE_THROWS_AS(topo.getLinksFromNode(99), std::out_of_range);
    REQUIRE_THROWS_AS(const_topo.getLinksFromNode(99), std::out_of_range);
}

TEST_CASE("Topology programmatic API validation", "[network][topology]")
{
    // Constructor negative node count
    REQUIRE_THROWS_AS(Topology(-1), std::invalid_argument);
    REQUIRE_THROWS_AS(Topology(-10), std::invalid_argument);

    Topology topo(3);

    // Reject negative node indices (both endpoints)
    REQUIRE_THROWS_AS(topo.addLink(-1, 1, 100.0, 10.0), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLink(0, -2, 100.0, 10.0), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLinkPtr(-1, 1, 100.0, 10.0), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLinkPtr(0, -2, 100.0, 10.0), std::invalid_argument);

    // Reject self-loops
    REQUIRE_THROWS_AS(topo.addLink(1, 1, 100.0, 10.0), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLinkPtr(2, 2, 100.0, 10.0), std::invalid_argument);

    // Reject non-positive bandwidth
    REQUIRE_THROWS_AS(topo.addLink(0, 1, 0.0, 10.0), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLink(0, 1, -50.0, 10.0), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLinkPtr(0, 1, 0.0, 10.0), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLinkPtr(0, 1, -10.0, 10.0), std::invalid_argument);

    // Reject negative delay
    REQUIRE_THROWS_AS(topo.addLink(0, 1, 10.0, -1.0), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLinkPtr(0, 1, 10.0, -5.0), std::invalid_argument);

    // Reject invalid loss probability
    REQUIRE_THROWS_AS(topo.addLink(0, 1, 100.0, 10.0, -0.1), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLink(0, 1, 100.0, 10.0, 1.5), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLinkPtr(0, 1, 100.0, 10.0, -0.01), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.addLinkPtr(0, 1, 100.0, 10.0, 2.0), std::invalid_argument);

    // Reject invalid global loss probability
    REQUIRE_THROWS_AS(topo.setGlobalLossProb(-0.1), std::invalid_argument);
    REQUIRE_THROWS_AS(topo.setGlobalLossProb(1.1), std::invalid_argument);

    // Valid parameters succeed
    REQUIRE_NOTHROW(topo.addLink(0, 1, 10.0, 5.0, 0.0));
    REQUIRE_NOTHROW(topo.addLinkPtr(1, 2, 10.0, 5.0, 1.0));
    REQUIRE_NOTHROW(topo.setGlobalLossProb(0.25));
}