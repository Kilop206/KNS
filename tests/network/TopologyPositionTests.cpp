#include <catch2/catch_test_macros.hpp>
#include "network/TopologyLoader.hpp"
#include <limits>

using namespace kns;

TEST_CASE("Canvas positions survive JSON round trips and run cloning", "[network][canvas]") {
    Topology topology(3);
    topology.addLink(0,1,100,1);
    const auto revision=topology.getRoutingRevision();
    REQUIRE(topology.setNodePosition(0,{-144.5,288.25}));
    REQUIRE(topology.setNodePosition(2,{0,0}));
    REQUIRE(topology.getRoutingRevision()==revision);
    topology.removeNode(2);
    const auto json=TopologyLoader::toJson(topology);
    REQUIRE_FALSE(json["nodes"][1].contains("position"));
    const auto restored=TopologyLoader::fromJson(json);
    REQUIRE(restored.getNode(0)->getPosition()==topology.getNode(0)->getPosition());
    REQUIRE(restored.getNode(2)->getPosition()==topology.getNode(2)->getPosition());
    REQUIRE_FALSE(restored.getNode(2)->isActive());
    auto clone=restored.cloneForRun();
    REQUIRE(clone.getNode(0)->getPosition()==restored.getNode(0)->getPosition());
    REQUIRE(clone.setNodePosition(0,{12,24}));
    REQUIRE(clone.getNode(0)->getPosition()!=restored.getNode(0)->getPosition());
}

TEST_CASE("Discovery updates preserve layout by external identity", "[network][canvas][discovery]") {
    auto document=nlohmann::json::parse(R"({"nodes":[
        {"id":0,"external_id":"device:a"},{"id":1,"external_id":"device:b"}],
        "links":[{"from":0,"to":1,"bandwidth":100,"delay":1,"loss":0}]})");
    auto topology=TopologyLoader::fromJson(document);
    topology.setNodePosition(0,{100,200});
    topology.setNodePosition(1,{-50,-80});
    document["nodes"][0]["id"]=1;
    document["nodes"][1]["id"]=0;
    auto snapshot=TopologyLoader::fromJson(document);
    topology.synchronizeFrom(snapshot);
    REQUIRE(topology.getNode(0)->getPosition()==std::optional<NodePosition>{{100,200}});
    REQUIRE(topology.getNode(1)->getPosition()==std::optional<NodePosition>{{-50,-80}});
    snapshot.setNodePosition(1,{24,48});
    REQUIRE(topology.synchronizeFrom(snapshot));
    REQUIRE(topology.getNode(0)->getPosition()==std::optional<NodePosition>{{24,48}});
    REQUIRE_FALSE(topology.synchronizeFrom(snapshot));
}

TEST_CASE("Invalid canvas coordinates are rejected without changing nodes", "[network][canvas]") {
    Topology topology(1);
    topology.setNodePosition(0,{12,24});
    for (double bad : {std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1000001.0,-1000001.0}) {
        REQUIRE_THROWS_AS(topology.setNodePosition(0,{bad,0}),std::invalid_argument);
        REQUIRE_THROWS_AS(topology.setNodePosition(0,{0,bad}),std::invalid_argument);
    }
    REQUIRE(topology.getNode(0)->getPosition()==std::optional<NodePosition>{{12,24}});
    const auto original=TopologyLoader::toJson(topology);
    for (const auto& value : {nlohmann::json(nullptr),nlohmann::json::array({1,2}),
        nlohmann::json{{"x",1}}, nlohmann::json{{"x","bad"},{"y",2}}, nlohmann::json{{"x",1e20},{"y",2}}}) {
        auto bad=original;
        bad["nodes"][0]["position"]=value;
        REQUIRE_THROWS_AS(TopologyLoader::fromJson(bad),std::invalid_argument);
    }
    REQUIRE_FALSE(topology.setNodePosition(8,{0,0}));
}
