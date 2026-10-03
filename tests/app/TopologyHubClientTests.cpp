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


TEST_CASE("Topology Hub client authenticates private reads and optimistic saves", "[hub][http]")
{
    httplib::Server server;
    std::string observedAuthorization;
    std::string observedHubHeader;
    std::uint64_t observedVersion = 99;

    const auto detail = [](std::uint64_t version, std::string title) {
        return nlohmann::json({
            {"topology", {
                {"title", std::move(title)},
                {"description", "Private topology"},
                {"visibility", "PRIVATE"},
                {"version", version}
            }},
            {"graph", {
                {"schema_version", "1.0"},
                {"name", "Private hub topology"},
                {"nodes", 2},
                {"links", nlohmann::json::array({
                    {{"from", 0}, {"to", 1}, {"bandwidth", 100.0}, {"delay", 1.0}, {"loss", 0.0}}
                })}
            }}
        });
    };

    server.Get("/api/topologies/private-1", [&](const httplib::Request& request, httplib::Response& response) {
        observedAuthorization = request.get_header_value("Authorization");
        response.set_content(detail(7, "Private network").dump(), "application/json");
    });
    server.Put("/api/topologies/private-1", [&](const httplib::Request& request, httplib::Response& response) {
        observedAuthorization = request.get_header_value("Authorization");
        observedHubHeader = request.get_header_value("X-Hub-Request");
        const auto body = nlohmann::json::parse(request.body);
        observedVersion = body.at("version").get<std::uint64_t>();
        REQUIRE(body.at("graph").at("schema_version") == "1.0");
        response.set_content(detail(8, body.at("title").get<std::string>()).dump(), "application/json");
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
    config.bearer_token = "knsh_test-token";
    TopologyHubClient client(config);

    auto document = client.fetchTopology("private-1");
    REQUIRE(document.version == 7);
    REQUIRE(document.title == "Private network");
    REQUIRE(document.topology.size() == 2);
    REQUIRE(observedAuthorization == "Bearer knsh_test-token");

    document.title = "Edited in KNS";
    auto saved = client.saveTopology(document);
    REQUIRE(observedAuthorization == "Bearer knsh_test-token");
    REQUIRE(observedHubHeader == "1");
    REQUIRE(observedVersion == 7);
    REQUIRE(saved.version == 8);
    REQUIRE(saved.title == "Edited in KNS");
}

TEST_CASE("Topology Hub client reports stale revisions and requires token for saves", "[hub][http]")
{
    kns::app::hub::HubTopology topology;
    topology.id = "private-1";
    topology.title = "Network";
    topology.description = "";
    topology.visibility = "PRIVATE";
    topology.version = 3;

    REQUIRE_THROWS_WITH(
        TopologyHubClient{}.saveTopology(topology),
        "Saving to Topology Hub requires KNS_TOPOLOGY_HUB_TOKEN"
    );

    httplib::Server server;
    server.Put("/api/topologies/private-1", [](const httplib::Request&, httplib::Response& response) {
        response.status = 409;
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
    config.bearer_token = "knsh_test-token";
    REQUIRE_THROWS_WITH(
        TopologyHubClient(config).saveTopology(topology),
        "Topology save failed because the topology changed on the Hub; reload before saving"
    );
}
