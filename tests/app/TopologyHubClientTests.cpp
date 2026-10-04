#include <catch2/catch_test_macros.hpp>

#include <future>
#include <chrono>
#include <stdexcept>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "hub/TopologyHubClient.hpp"
#include "include/Environment.hpp"

using kns::app::hub::TopologyHubClient;
using kns::app::hub::TopologyHubClientConfig;

TEST_CASE("Topology Hub client uploads local topology and reuses its server ID", "[hub][http]")
{
    httplib::Server server;
    nlohmann::json uploaded;
    std::string authorization, hubHeader;
    server.Post("/api/topologies", [&](const httplib::Request& request, httplib::Response& response) {
        uploaded = nlohmann::json::parse(request.body);
        authorization = request.get_header_value("Authorization");
        hubHeader = request.get_header_value("X-Hub-Request");
        auto summary = uploaded;
        summary.erase("graph");
        summary["id"] = "created-1";
        summary["version"] = 0;
        response.status = 201;
        response.set_content(nlohmann::json({{"topology", summary}, {"graph", uploaded.at("graph")}}).dump(), "application/json");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    auto worker = std::async(std::launch::async, [&] { server.listen_after_bind(); });
    struct StopServer { httplib::Server& server; ~StopServer() { server.stop(); } } cleanup{server};
    server.wait_until_ready();

    TopologyHubClientConfig config;
    config.base_url = "http://127.0.0.1:" + std::to_string(port);
    config.bearer_token = "knsh_test-token";
    kns::app::hub::HubTopology local;
    local.title = "Local network";
    local.description = "Uploaded from KNS";
    local.visibility = "PRIVATE";
    local.topology = kns::Topology(2);
    local.topology.addLink(0, 1, 100, 5);
    REQUIRE_THROWS(TopologyHubClient{}.createTopology(local));
    const auto saved = TopologyHubClient(config).createTopology(local);
    REQUIRE(saved.id == "created-1");
    REQUIRE(saved.version == 0);
    REQUIRE(saved.title == local.title);
    REQUIRE(saved.topology.getLinks().size() == 1);
    REQUIRE(authorization == "Bearer knsh_test-token");
    REQUIRE(hubHeader == "1");
    REQUIRE_FALSE(uploaded.contains("version"));
    REQUIRE(uploaded.at("graph").at("schema_version") == "1.0");
}

TEST_CASE("KNS uploads and updates a topology on a running Hub", "[.][hub-live]")
{
    const auto url = kns::app::readEnvironmentVariable("KNS_HUB_TEST_URL");
    if (!url) SKIP("Set KNS_HUB_TEST_URL to an isolated development Hub");
    httplib::Client browser(*url);
    browser.set_read_timeout(20);
    const auto suffix = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    httplib::Headers headers{{"X-Hub-Request", "1"}};
    const auto registration = browser.Post("/api/auth/register", headers,
        nlohmann::json({{"displayName", "KNS integration test"}, {"email", "kns-upload-" + suffix + "@test.local"}, {"password", "Integration-test-" + suffix}}).dump(), "application/json");
    REQUIRE(registration);
    REQUIRE(registration->status == 201);
    const auto cookie = registration->get_header_value("Set-Cookie");
    headers.emplace("Cookie", cookie.substr(0, cookie.find(';')));
    const auto credential = browser.Post("/api/auth/tokens", headers, R"({"name":"KNS integration test"})", "application/json");
    REQUIRE(credential);
    REQUIRE(credential->status == 201);
    const auto token = nlohmann::json::parse(credential->body);
    TopologyHubClientConfig config;
    config.base_url = *url;
    config.bearer_token = token.at("token").get<std::string>();
    TopologyHubClient client(config);
    kns::app::hub::HubTopology local;
    local.title = "KNS upload integration test";
    local.visibility = "PRIVATE";
    local.topology = kns::Topology(2);
    local.topology.addLink(0, 1, 100, 5);
    auto saved = client.createTopology(local);
    struct Cleanup {
        httplib::Client& browser;
        httplib::Headers& headers;
        std::string topologyId, tokenId;
        ~Cleanup() {
            browser.Delete("/api/topologies/" + topologyId, headers);
            browser.Delete("/api/auth/tokens/" + tokenId, headers);
            browser.Post("/api/auth/logout", headers, "{}", "application/json");
        }
    } cleanup{browser, headers, saved.id, token.at("id").get<std::string>()};
    REQUIRE_FALSE(saved.id.empty());
    REQUIRE(saved.version == 0);
    REQUIRE(client.fetchTopology(saved.id).topology.size() == 2);
    saved.title = "Updated from KNS";
    const auto updated = client.saveTopology(saved);
    REQUIRE(updated.id == saved.id);
    REQUIRE(updated.version == 1);
    REQUIRE(client.fetchTopology(saved.id).title == "Updated from KNS");
    const auto revoked = browser.Delete("/api/auth/tokens/" + token.at("id").get<std::string>(), headers);
    REQUIRE(revoked);
    REQUIRE(revoked->status == 204);
    REQUIRE_THROWS(client.createTopology(local));
}

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
    bool observedSchemaVersion = false;

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
        observedSchemaVersion =
            body.at("graph").at("schema_version").get<std::string>() == "1.0";
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
    REQUIRE(observedSchemaVersion);
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

    std::string missingTokenMessage;
    try {
        TopologyHubClient{}.saveTopology(topology);
    } catch (const std::runtime_error& error) {
        missingTokenMessage = error.what();
    }
    REQUIRE(
        missingTokenMessage ==
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
    std::string staleRevisionMessage;
    try {
        TopologyHubClient(config).saveTopology(topology);
    } catch (const std::runtime_error& error) {
        staleRevisionMessage = error.what();
    }
    REQUIRE(
        staleRevisionMessage ==
        "Topology save failed because the topology changed on the Hub; reload before saving"
    );
}
