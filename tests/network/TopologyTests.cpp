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
