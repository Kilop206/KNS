#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <thread>
#include "intelligence/ChatService.hpp"
#include "intelligence/IntelligenceClient.hpp"
#include <httplib.h>

using kns::app::intelligence::ChatService;

namespace {
void complete(ChatService& chat)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (chat.busy() && std::chrono::steady_clock::now() < deadline) {
        chat.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE_FALSE(chat.busy());
}
}

TEST_CASE("KiWi chat sends topology and bounded alternating history", "[chat]")
{
    nlohmann::json captured;
    ChatService chat([&](const nlohmann::json& request) { captured = request; return "KiWi reply"; });
    kns::analysis::NetworkAnalysis analysis;
    analysis.node_count = 3;
    for (int id : {10, 30, 50}) {
        kns::analysis::NodeMetrics node;
        node.node_id = id;
        analysis.nodes.push_back(node);
    }
    chat.synchronizeTopology(7);
    for (int i = 0; i < 12; ++i) {
        chat.send(analysis, "Question " + std::to_string(i));
        complete(chat);
    }
    REQUIRE(captured.at("topologyRevision") == "7");
    REQUIRE(captured.at("analysisMode") == "deep");
    REQUIRE(captured.at("context").is_object());
    REQUIRE(captured.at("context").at("network").at("node_ids") == nlohmann::json({10, 30, 50}));
    REQUIRE(captured.at("score").contains("overall"));
    REQUIRE(captured.at("messages").size() == 21);
    REQUIRE(captured.at("messages").front().at("content") == "Question 1");
    REQUIRE(captured.at("messages").back().at("content") == "Question 11");
    REQUIRE(chat.messages().size() == 24);
}

TEST_CASE("KiWi chat keeps failed turn for retry without duplicate messages", "[chat]")
{
    int calls = 0;
    ChatService chat([&](const nlohmann::json&) -> std::string {
        if (++calls == 1) throw std::runtime_error("offline");
        return "Recovered";
    });
    chat.send({}, "  Question  ");
    complete(chat);
    REQUIRE(chat.error() == "offline");
    REQUIRE(chat.messages().size() == 1);
    REQUIRE(chat.canRetry());
    chat.send({}, "must not replace failed turn");
    REQUIRE(chat.messages().size() == 1);
    chat.retry();
    complete(chat);
    REQUIRE(chat.messages().size() == 2);
    REQUIRE(chat.messages().front().content == "Question");
    REQUIRE(chat.error().empty());
    REQUIRE_FALSE(chat.canRetry());
}

TEST_CASE("KiWi chat discards pending replies on topology change or clear", "[chat]")
{
    std::promise<void> release;
    auto ready = release.get_future().share();
    ChatService chat([ready](const nlohmann::json&) { ready.wait(); return "Old reply"; });
    chat.send({}, "Old question");
    const bool wasBusy = chat.busy();
    SECTION("new topology") { chat.synchronizeTopology(1); }
    SECTION("new conversation") { chat.clear(); }
    release.set_value(); // release worker even if subsequent assertions fail
    REQUIRE(wasBusy);
    complete(chat);
    REQUIRE(chat.messages().empty());
    REQUIRE(chat.error().empty());
    REQUIRE_FALSE(chat.canRetry());
}

TEST_CASE("KiWi chat ignores blank questions and reports oversized input", "[chat]")
{
    ChatService chat([](const nlohmann::json&) { return "unexpected"; });
    chat.send({}, " \n\t ");
    REQUIRE_FALSE(chat.busy());
    REQUIRE(chat.messages().empty());
    chat.send({}, std::string(4097, 'x'));
    REQUIRE_FALSE(chat.error().empty());
    REQUIRE(chat.messages().empty());
}

TEST_CASE("KiWi chat allows editing a failed question and forgets stale errors", "[chat]")
{
    ChatService chat([](const nlohmann::json&) -> std::string { throw std::runtime_error("offline"); });
    chat.send({}, "Original question");
    complete(chat);
    chat.discardFailedQuestion();
    REQUIRE(chat.messages().empty());
    REQUIRE(chat.error().empty());
    REQUIRE_FALSE(chat.canRetry());
    chat.send({}, "Edited question");
    chat.synchronizeTopology(2);
    complete(chat);
    REQUIRE(chat.messages().empty());
    REQUIRE(chat.error().empty());
    REQUIRE_FALSE(chat.canRetry());
}

TEST_CASE("KiWi HTTP transport validates request identity and errors", "[chat][http]")
{
    httplib::Server server;
    std::string mode = "success";
    server.Post("/chat", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.get_header_value("Authorization") != "Bearer test-token") { res.status = 401; return; }
        auto body = nlohmann::json::parse(req.body);
        if (mode == "offline") { res.status = 503; return; }
        if (mode == "invalid") { res.set_content("not json", "application/json"); return; }
        res.set_content(nlohmann::json({{"requestId", mode == "stale" ? "wrong" : body.at("requestId").get<std::string>()},
            {"topologyRevision", body.at("topologyRevision")}, {"message", "Reply"}}).dump(), "application/json");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    auto worker = std::async(std::launch::async, [&] { server.listen_after_bind(); });
    struct StopServer { httplib::Server& server; ~StopServer() { server.stop(); } } cleanup{server};
    server.wait_until_ready();
    kns::app::intelligence::IntelligenceClientConfig config;
    config.base_url = "http://127.0.0.1:" + std::to_string(port);
    config.chat_endpoint = "/chat";
    config.bearer_token = "test-token";
    kns::app::intelligence::IntelligenceClient client(config);
    const nlohmann::json request{{"requestId", "turn-1"}, {"topologyRevision", "3"}};
    REQUIRE(client.chat(request) == "Reply");
    mode = "stale";
    REQUIRE_THROWS(client.chat(request));
    mode = "invalid";
    REQUIRE_THROWS(client.chat(request));
    mode = "offline";
    REQUIRE_THROWS(client.chat(request));
}
