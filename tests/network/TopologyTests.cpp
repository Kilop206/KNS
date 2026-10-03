#include <catch2/catch_test_macros.hpp>
#include "network/Topology.hpp"
#include "network/Link.hpp"

using namespace kns;

TEST_CASE("Topology add/remove nodes and links", "[network][topology]") {
    Topology topo(2);
    REQUIRE(topo.size() == 2);

    int n3 = topo.addNode();
    REQUIRE(n3 == 2);
    REQUIRE(topo.size() == 3);

    auto link = topo.addLinkPtr(0, 2, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
    REQUIRE(link != nullptr);

    const auto& links_from_0 = topo.getLinksFromNode(0);
    bool found = false;
    for (const auto& l : links_from_0) {
        if (l->getOtherNode(0) == 2) { found = true; break; }
    }
    REQUIRE(found);

    // toggle down
    REQUIRE(topo.setLinkUp(0,2, false) == true);
    REQUIRE(link->isUp() == false);

    // toggle up
    REQUIRE(topo.setLinkUp(0,2, true) == true);
    REQUIRE(link->isUp() == true);

    // remove link
    REQUIRE(topo.removeLink(0,2) == true);

    // after removal, adjacency list for node 0 should not contain the link
    const auto& links_after = topo.getLinksFromNode(0);
    bool still = false;
    for (const auto& l : links_after) {
        if (l->getOtherNode(0) == 2) { still = true; break; }
    }
    REQUIRE(still == false);

    // remove node
    REQUIRE(topo.removeNode(2) == true);
    REQUIRE(topo.getLinksFromNode(2).empty());
}

TEST_CASE("Topology requires link ids for ambiguous parallel-link mutations", "[network][topology]") {
    Topology topo(2);
    auto first = topo.addLinkPtr(0, 1, 10.0, 5.0);
    auto second = topo.addLinkPtr(0, 1, 20.0, 10.0);

    REQUIRE_FALSE(topo.setLinkUp(0, 1, false));
    REQUIRE(first->isUp());
    REQUIRE(second->isUp());

    REQUIRE(topo.setLinkUpById(first->getId(), false));
    REQUIRE_FALSE(first->isUp());
    REQUIRE(second->isUp());

    REQUIRE_FALSE(topo.removeLink(0, 1));
    REQUIRE(topo.getLinks().size() == 2);

    REQUIRE(topo.removeLinkById(second->getId()));
    REQUIRE(topo.getLinks().size() == 1);
    REQUIRE(topo.getLinks().front()->getId() == first->getId());
    REQUIRE(topo.getInterfaces().size() == 2);
    REQUIRE(topo.getInterfaces().front().getLinkId() == first->getId());
    REQUIRE_FALSE(topo.removeLinkById(second->getId()));

    REQUIRE(topo.removeLink(0, 1));
    REQUIRE(topo.getLinks().empty());
}
