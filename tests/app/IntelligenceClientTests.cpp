#include <catch2/catch_test_macros.hpp>

#include <future>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "intelligence/IntelligenceClient.hpp"
#include "intelligence/IntelligenceRequest.hpp"

TEST_CASE("Intelligence analyze forwards request id as HTTP correlation header", "[intelligence][http]")
{
    httplib::Server server;
    std::string observedRequestId;

    server.Post("/analyze", [&](const httplib::Request& request, httplib::Response& response) {
        observedRequestId = request.get_header_value("X-Request-ID");
        response.set_content(
            nlohmann::json{
                {"schema_version", "1.0"},
                {"analysis_id", "analysis-1"},
                {"status", "completed"},
                {"network_score", 80.0},
                {"summary", "Analysis complete."},
                {"findings", nlohmann::json::array()},
                {"recommendations", nlohmann::json::array()}
            }.dump(),
            "application/json"
        );
    });

    const int port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    auto worker = std::async(std::launch::async, [&] { server.listen_after_bind(); });
    struct StopServer {
        httplib::Server& server;
        ~StopServer() { server.stop(); }
    } cleanup{server};
    server.wait_until_ready();

    kns::app::intelligence::IntelligenceClientConfig config;
    config.base_url = "http://127.0.0.1:" + std::to_string(port);
    config.analyze_endpoint = "/analyze";

    kns::intelligence::IntelligenceRequest request;
    request.request_id = "analysis-req-42";
    request.context = nlohmann::json::object();

    const auto response =
        kns::app::intelligence::IntelligenceClient(config).analyze(request);

    REQUIRE(observedRequestId == "analysis-req-42");
    REQUIRE(response.analysis_id == "analysis-1");
}
