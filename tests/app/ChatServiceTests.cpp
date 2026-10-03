#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <thread>
#include "intelligence/ChatService.hpp"
#include "intelligence/IntelligenceClient.hpp"
#include <httplib.h>

using kns::app::intelligence::ChatService;
using kns::app::intelligence::ChatReply;

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
    ChatService chat([&](const nlohmann::json& request) { captured = request; return ChatReply{"KiWi reply"}; });
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
    REQUIRE(chat.messages().back().history_turns_omitted == 1);
}

TEST_CASE("KiWi chat keeps failed turn for retry without duplicate messages", "[chat]")
{
    int calls = 0;
    ChatService chat([&](const nlohmann::json&) -> ChatReply {
        if (++calls == 1) throw std::runtime_error("offline");
        return {"Recovered"};
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
    ChatService chat([ready](const nlohmann::json&) { ready.wait(); return ChatReply{"Old reply"}; });
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
    ChatService chat([](const nlohmann::json&) { return ChatReply{"unexpected"}; });
    chat.send({}, " \n\t ");
    REQUIRE_FALSE(chat.busy());
    REQUIRE(chat.messages().empty());
    chat.send({}, std::string(4097, 'x'));
    REQUIRE_FALSE(chat.error().empty());
    REQUIRE(chat.messages().empty());
}

TEST_CASE("KiWi chat allows editing a failed question and forgets stale errors", "[chat]")
{
    ChatService chat([](const nlohmann::json&) -> ChatReply { throw std::runtime_error("offline"); });
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

TEST_CASE("KiWi chat reports client and model history omissions without deleting the transcript", "[chat]")
{
    nlohmann::json captured;
    bool fail = false;
    ChatService chat([&](const nlohmann::json& request) -> ChatReply {
        if (fail) throw std::runtime_error("offline");
        captured = request;
        const auto turns = request.at("messages").size() / 2;
        return {std::string(16000, 'a'), turns > 0 ? 1u : 0u};
    });
    for (int i = 0; i < 5; ++i) {
        chat.send({}, "Question " + std::to_string(i));
        complete(chat);
    }
    // The byte budget omits one old turn; the model omits another.
    REQUIRE(captured.at("messages").size() == 7);
    REQUIRE(chat.messages().size() == 10);
    REQUIRE(chat.messages().front().content == "Question 0");
    REQUIRE(chat.messages().back().history_turns_omitted == 2);
    fail = true;
    chat.send({}, "Retry me");
    complete(chat);
    REQUIRE(chat.canRetry());
    fail = false;
    chat.retry();
    complete(chat);
    REQUIRE(chat.messages().size() == 12);
    REQUIRE(chat.messages().back().history_turns_omitted == 3);
    chat.clear();
    chat.send({}, "Fresh question");
    complete(chat);
    REQUIRE(chat.messages().back().history_turns_omitted == 0);
}

TEST_CASE("KiWi chat preserves a failed question when history metadata is impossible", "[chat]")
{
    ChatService chat([](const nlohmann::json&) { return ChatReply{"Reply", 1}; });
    chat.send({}, "First question");
    complete(chat);
    REQUIRE(chat.canRetry());
    REQUIRE(chat.messages().size() == 1);
    REQUIRE_FALSE(chat.error().empty());
}

TEST_CASE("KiWi transcript preserves full UTF-8 history and context notices", "[chat]")
{
    ChatService chat([](const nlohmann::json& request) {
        return ChatReply{"Verifique a conexão.\nNão há telemetria.", request.at("messages").size() > 1 ? 1u : 0u};
    });
    REQUIRE(chat.transcript().empty());
    chat.synchronizeTopology(42);
    chat.send({}, "E a redundância?\nRota 10 → 30");
    complete(chat);
    chat.send({}, "Pode explicar?");
    complete(chat);
    const auto transcript = chat.transcript();
    REQUIRE(transcript.find("revisão da topologia: 42") != std::string::npos);
    REQUIRE(transcript.find("Você:\nE a redundância?\nRota 10 → 30") != std::string::npos);
    REQUIRE(transcript.find("KiWi:\nVerifique a conexão.\nNão há telemetria.") != std::string::npos);
    REQUIRE(transcript.find("fora do contexto desta resposta: 1") != std::string::npos);
    REQUIRE(transcript.find("ainda sem resposta") == std::string::npos);
    REQUIRE(chat.messages().size() == 4);
    chat.synchronizeTopology(43);
    REQUIRE(chat.transcript().empty());
}

TEST_CASE("KiWi transcript identifies pending and failed questions without inventing replies", "[chat]")
{
    std::promise<void> release;
    auto ready = release.get_future().share();
    ChatService chat([ready](const nlohmann::json&) -> ChatReply {
        ready.wait();
        throw std::runtime_error("private transport diagnostic");
    });
    chat.send({}, "Pending question");
    const auto pending = chat.transcript();
    release.set_value();
    complete(chat);
    REQUIRE(pending.find("Última pergunta ainda sem resposta") != std::string::npos);
    REQUIRE(pending.find("KiWi:\n") == std::string::npos);
    REQUIRE(chat.transcript() == pending);
    REQUIRE(chat.canRetry());
    chat.discardFailedQuestion();
    REQUIRE(chat.transcript().empty());
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
        auto reply = nlohmann::json({{"requestId", mode == "stale" ? "wrong" : body.at("requestId").get<std::string>()},
            {"topologyRevision", body.at("topologyRevision")}, {"message", "Reply"}});
        if (mode != "success" && mode != "stale") reply["historyTurnsOmitted"] = nlohmann::json::parse(mode);
        res.set_content(reply.dump(), "application/json");
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
    const nlohmann::json request{{"requestId", "turn-1"}, {"topologyRevision", "3"},
        {"messages", {{{"role", "user"}, {"content", "Earlier"}},
                      {{"role", "assistant"}, {"content", "Reply"}},
                      {{"role", "user"}, {"content", "Now?"}}}}};
    REQUIRE(client.chat(request).message == "Reply");
    REQUIRE(client.chat(request).history_turns_omitted == 0);
    mode = "1";
    REQUIRE(client.chat(request).history_turns_omitted == 1);
    for (const auto* value : {"-1", "2", "1.5", "1.0", "true", "null", "\"1\"", "4294967296"}) {
        mode = value;
        REQUIRE_THROWS(client.chat(request));
    }
    mode = "stale";
    REQUIRE_THROWS(client.chat(request));
    mode = "invalid";
    REQUIRE_THROWS(client.chat(request));
    mode = "offline";
    REQUIRE_THROWS(client.chat(request));
}
