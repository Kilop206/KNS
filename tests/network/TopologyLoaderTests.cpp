#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "network/TopologyLoader.hpp"
#include "network/Topology.hpp"
#include "enums/LinkMode.hpp"
#include <fstream>
#include <filesystem>
#include <stdexcept>

using kns::TopologyLoader;
using kns::Topology;
using kns::LinkMode;

namespace fs = std::filesystem;

struct TempJsonFile {
    std::string path;
    TempJsonFile(const std::string& filename, const std::string& content) : path(filename) {
        std::ofstream out(path);
        out << content;
    }
    ~TempJsonFile() {
        if (fs::exists(path)) {
            fs::remove(path);
        }
    }
};

TEST_CASE("Topology JSON validates optional queue capacity", "[network][loader][queue]")
{
    for (const auto capacity : {"1", "5", "0", "-1", "1.5", "2147483648", "\"4\""}) {
        CAPTURE(capacity);
        const std::string content = std::string(R"({"nodes":2,"links":[{"from":0,"to":1,"bandwidth":10,"delay":1,"loss":0,"queue_capacity":)") + capacity + "}]}";
        TempJsonFile file("temp_queue_capacity.json", content);
        if (std::string(capacity) == "1" || std::string(capacity) == "5") {
            const auto topology = TopologyLoader::load_topology(file.path);
            REQUIRE(topology.getLinks().front()->getQueueCapacity() == std::stoul(capacity));
        } else {
            REQUIRE_THROWS_AS(TopologyLoader::load_topology(file.path), std::invalid_argument);
        }
    }
}

TEST_CASE("TopologyLoader parses valid topologies with mode and isolated nodes", "[network][loader]")
{
    std::string json_content = R"({
        "name": "custom_topo",
        "nodes": 5,
        "links": [
            {
                "from": 0,
                "to": 1,
                "bandwidth": 100.0,
                "delay": 10.0,
                "loss": 0.05,
                "mode": "half_duplex"
            },
            {
                "from": 1,
                "to": 2,
                "bandwidth": 50.0,
                "delay": 20.0,
                "loss": 0.0,
                "mode": "simplex"
            },
            {
                "from": 2,
                "to": 3,
                "bandwidth": 1000.0,
                "delay": 2.0,
                "loss": 0.0,
                "mode": "full_duplex"
            }
        ]
    })";

    TempJsonFile temp_file("temp_valid_topo.json", json_content);
    Topology topo = TopologyLoader::load_topology(temp_file.path);

    REQUIRE(topo.size() == 5);
    REQUIRE(std::string(topo.getName()) == "custom_topo");

    const auto& links_from_0 = topo.getLinksFromNode(0);
    REQUIRE(links_from_0.size() == 1);
    REQUIRE(links_from_0[0]->getMode() == LinkMode::HALF_DUPLEX);
    REQUIRE(links_from_0[0]->getLossProb() == 0.05);

    const auto& links_from_1 = topo.getLinksFromNode(1);
    REQUIRE(links_from_1.size() == 2);
    REQUIRE(links_from_1[1]->getMode() == LinkMode::SIMPLEX);

    REQUIRE(topo.getLinksFromNode(4).empty());
}

TEST_CASE("TopologyLoader rejects invalid JSON and throws descriptive exceptions", "[network][loader]")
{
    SECTION("File does not exist") {
        REQUIRE_THROWS_AS(TopologyLoader::load_topology("non_existent_file_12345.json"), std::runtime_error);
    }

    SECTION("Missing required link field (e.g. bandwidth)") {
        TempJsonFile temp("temp_missing_field.json", R"({
            "name": "invalid",
            "links": [{"from": 0, "to": 1, "delay": 10, "loss": 0.0}]
        })");
        REQUIRE_THROWS_AS(TopologyLoader::load_topology(temp.path), std::invalid_argument);
    }

    SECTION("Non-positive bandwidth (prevents division by zero)") {
        TempJsonFile temp("temp_zero_bw.json", R"({
            "name": "invalid",
            "links": [{"from": 0, "to": 1, "bandwidth": 0.0, "delay": 10, "loss": 0.0}]
        })");
        REQUIRE_THROWS_AS(TopologyLoader::load_topology(temp.path), std::invalid_argument);
    }

    SECTION("Negative loss or loss > 1.0") {
        TempJsonFile temp("temp_invalid_loss.json", R"({
            "name": "invalid",
            "links": [{"from": 0, "to": 1, "bandwidth": 100.0, "delay": 10, "loss": 1.5}]
        })");
        REQUIRE_THROWS_AS(TopologyLoader::load_topology(temp.path), std::invalid_argument);
    }

    SECTION("Self-loop link (from == to)") {
        TempJsonFile temp("temp_self_loop.json", R"({
            "name": "invalid",
            "links": [{"from": 1, "to": 1, "bandwidth": 100.0, "delay": 10, "loss": 0.0}]
        })");
        REQUIRE_THROWS_AS(TopologyLoader::load_topology(temp.path), std::invalid_argument);
    }

    SECTION("Negative node indices") {
        TempJsonFile temp("temp_neg_node.json", R"({
            "name": "invalid",
            "links": [{"from": -1, "to": 2, "bandwidth": 100.0, "delay": 10, "loss": 0.0}]
        })");
        REQUIRE_THROWS_AS(TopologyLoader::load_topology(temp.path), std::invalid_argument);
    }

    SECTION("Unknown link mode") {
        TempJsonFile temp("temp_unknown_mode.json", R"({
            "name": "invalid",
            "links": [{"from": 0, "to": 1, "bandwidth": 100.0, "delay": 10, "loss": 0.0, "mode": "wireless_magic"}]
        })");
        REQUIRE_THROWS_AS(TopologyLoader::load_topology(temp.path), std::invalid_argument);
    }
}


TEST_CASE("Topology JSON loads and preserves RED queue policy", "[network][loader][queue][red]")
{
    TempJsonFile file("temp_red_queue.json", R"({
      "schema_version":"1.0",
      "nodes":2,
      "links":[{
        "from":0,"to":1,"bandwidth":100,"delay":1,"loss":0,
        "queue_capacity":16,
        "queue_policy":"red",
        "red_min_threshold":4,
        "red_max_threshold":12,
        "red_max_drop_probability":0.25
      }]
    })");

    const auto topology = TopologyLoader::load_topology(file.path);
    const auto link = topology.getLinks().front();
    REQUIRE(link->getQueueDiscipline() == kns::Link::QueueDiscipline::RED);
    REQUIRE(link->getRedMinThreshold() == 4);
    REQUIRE(link->getRedMaxThreshold() == 12);
    REQUIRE(link->getRedMaxDropProbability() == Catch::Approx(0.25));

    const auto serialized = TopologyLoader::toJson(topology);
    const auto& saved = serialized.at("links").at(0);
    REQUIRE(saved.at("queue_policy") == "red");
    REQUIRE(saved.at("red_min_threshold") == 4);
    REQUIRE(saved.at("red_max_threshold") == 12);
    REQUIRE(saved.at("red_max_drop_probability").get<double>() == Catch::Approx(0.25));
}

TEST_CASE("Topology JSON rejects malformed RED queue policies", "[network][loader][queue][red]")
{
    for (const auto* linkConfig : {
        R"("queue_policy":"red")",
        R"("queue_policy":"magic")",
        R"("queue_policy":"red","red_min_threshold":4,"red_max_threshold":4,"red_max_drop_probability":0.2)",
        R"("queue_policy":"red","red_min_threshold":4,"red_max_threshold":20,"red_max_drop_probability":0.2)",
        R"("queue_policy":"red","red_min_threshold":1,"red_max_threshold":4,"red_max_drop_probability":2.0)"
    }) {
        CAPTURE(linkConfig);
        const std::string content =
            R"({"nodes":2,"links":[{"from":0,"to":1,"bandwidth":100,"delay":1,"loss":0,"queue_capacity":8,)" +
            std::string(linkConfig) + "}]}";
        TempJsonFile file("temp_bad_red_queue.json", content);
        REQUIRE_THROWS_AS(TopologyLoader::load_topology(file.path), std::invalid_argument);
    }
}
