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

TEST_CASE("Link queue management limits", "[network][link]")
{
    Link link(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);

    REQUIRE(link.getQueueSize() == 0);
    REQUIRE(link.getQueueCapacity() == 32);
    REQUIRE(link.canQueue());

    for (std::size_t i = 0; i < 32; ++i) {
        REQUIRE(link.canQueue());
        link.enqueuePacket();
    }

    REQUIRE_FALSE(link.canQueue());
    REQUIRE(link.getQueueSize() == 32);

    link.dequeuePacket();
    REQUIRE(link.canQueue());
    REQUIRE(link.getQueueSize() == 31);
}

TEST_CASE("Link drop behavior based on probability", "[network][link]")
{
    Link link_no_loss(1, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    REQUIRE_FALSE(link_no_loss.should_drop());

    Link link_total_loss(1, 2, 10.0, 5.0, 1.0, LinkMode::FULL_DUPLEX);
    REQUIRE(link_total_loss.should_drop());
}

TEST_CASE("Topology getLinksFromNode bounds safety", "[network][topology]")
{
    Topology topo(2);
    const auto& const_topo = topo;
    REQUIRE(const_topo.getLinksFromNode(-1).empty());
    REQUIRE(const_topo.getLinksFromNode(99).empty());
    REQUIRE_THROWS_AS(topo.getLinksFromNode(-1), std::out_of_range);
    REQUIRE_THROWS_AS(topo.getLinksFromNode(99), std::out_of_range);
}