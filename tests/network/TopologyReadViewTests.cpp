#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include <utility>
#include "engine/core/SimulationEngine.hpp"

using namespace kns;

static_assert(std::is_same_v<decltype(std::declval<Topology&>().getNode(0)), const Node*>);
static_assert(std::is_same_v<decltype(std::declval<Topology&>().getLinksFromNode(0)),
    const std::vector<Topology::LinkPtr>&>);

TEST_CASE("Supported topology removal preserves inventory and refreshes routes", "[network][topology][views]")
{
    Topology initial(3);
    initial.addLink(0, 1, 10.0, 1.0);
    initial.addLink(1, 2, 10.0, 1.0);
    SimulationEngine engine(initial);
    auto& topology = engine.getTopology();
    REQUIRE(engine.getNextHop(0, 2) == 1);
    REQUIRE(topology.getLinks().size() == 2);
    REQUIRE(topology.getInterfaces().size() == 4);
    const auto revision = topology.getRoutingRevision();
    REQUIRE(topology.removeNode(1));
    REQUIRE_FALSE(topology.getNode(1)->isActive());
    REQUIRE(topology.getRoutingRevision() > revision);
    REQUIRE(topology.getLinks().empty());
    REQUIRE(topology.getInterfaces().empty());
    for (int id = 0; id < 3; ++id) {
        REQUIRE(topology.getLinksFromNode(id).empty());
    }
    REQUIRE(engine.getNextHop(0, 2) == -1);
    REQUIRE(topology.getNode(-1) == nullptr);
    REQUIRE(topology.getNode(3) == nullptr);
}

TEST_CASE("Node labels change only through active topology nodes", "[network][topology][views]")
{
    Topology topology(2);
    const auto revision = topology.getRoutingRevision();
    REQUIRE(topology.setNodeLabel(0, "source"));
    REQUIRE(topology.getNode(0)->getLabel() == "source");
    REQUIRE(topology.getRoutingRevision() == revision);
    REQUIRE_FALSE(topology.setNodeLabel(-1, "invalid"));
    REQUIRE_FALSE(topology.setNodeLabel(2, "invalid"));
    REQUIRE(topology.removeNode(0));
    REQUIRE_FALSE(topology.setNodeLabel(0, "inactive"));
    REQUIRE(topology.getNode(0)->getLabel() == "source");
}
