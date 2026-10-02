#include <catch2/catch_test_macros.hpp>

#include <future>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "hub/TopologyHubClient.hpp"

using kns::app::hub::TopologyHubClient;
using kns::app::hub::TopologyHubClientConfig;

TEST_CASE("Topology Hub client loads a public KNS topology", "[hub][http]")
{
    httplib::Server server;
    server.Get("/api/topologies/network-1/download", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(nlohmann::json({
            {"schema_version", "1.0"},
            {"name", "Hub topology"},
            {"nodes", 2},
            {"links", nlohmann::json::array({
                {{"from", 0}, {"to", 1}, {"bandwidth", 100.0}, {"delay", 2.5}, {"loss", 0.01}}
            })}
        }).dump(), "application/json");
    });

    const int port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    auto worker = std::async(std::launch::async, [&] { server.listen_after_bind(); });
    struct StopServer {
        httplib::Server& server;
        ~StopServer() { server.stop(); }
    } cleanup{server};
    server.wait_until_ready();

    TopologyHubClientConfig config;
    config.base_url = "http://127.0.0.1:" + std::to_string(port);
    TopologyHubClient client(config);

    const auto topology = client.fetchPublicTopology("network-1");
    REQUIRE(topology.getName() == "Hub topology");
    REQUIRE(topology.size() == 2);
    REQUIRE(topology.getLinks().size() == 1);
    REQUIRE(topology.getLinks().front()->getDelayMs() == 2.5);
}

TEST_CASE("Topology Hub client rejects invalid IDs and upstream failures", "[hub][http]")
{
    REQUIRE_THROWS_AS(
        TopologyHubClient{}.fetchPublicTopology("../private"),
        std::invalid_argument
    );

    httplib::Server server;
    server.Get("/api/topologies/missing/download", [](const httplib::Request&, httplib::Response& response) {
        response.status = 404;
    });
    server.Get("/api/topologies/broken/download", [](const httplib::Request&, httplib::Response& response) {
        response.set_content("{", "application/json");
    });

    const int port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    auto worker = std::async(std::launch::async, [&] { server.listen_after_bind(); });
    struct StopServer {
        httplib::Server& server;
        ~StopServer() { server.stop(); }
    } cleanup{server};
    server.wait_until_ready();

    TopologyHubClientConfig config;
    config.base_url = "http://127.0.0.1:" + std::to_string(port);
    TopologyHubClient client(config);

    REQUIRE_THROWS(client.fetchPublicTopology("missing"));
    REQUIRE_THROWS(client.fetchPublicTopology("broken"));
}
