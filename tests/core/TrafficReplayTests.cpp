#include <catch2/catch_test_macros.hpp>
#include "traffic/TrafficRun.hpp"
#include "engine/core/SimulationEngine.hpp"
#include <limits>

namespace {
using Json = nlohmann::json;
Json plan() {
    return {{"schema_version", "1.0"}, {"seed", 42}, {"max_packets", 10},
        {"flows", {{{"id", 1}, {"source", 0}, {"destination", 1}, {"wire_bytes", 125},
                    {"start_s", 0}, {"end_s", 1}, {"schedule", {{"kind", "replay"}}}},
                   {{"id", 2}, {"source", 0}, {"destination", 1}, {"wire_bytes", 250},
                    {"start_s", 0}, {"end_s", 1}, {"schedule", {{"kind", "replay"}}}}}},
        {"replay_events", {{{"at_s", 0}, {"flow_id", 1}}, {{"at_s", 0}, {"flow_id", 1}},
                           {{"at_s", 0}, {"flow_id", 2}}, {{"at_s", 0.01}, {"flow_id", 1}}}}};
}
kns::Topology topology() { kns::Topology t(2); t.addLink(0, 1, 1, 1); return t; }
}
TEST_CASE("Replay preserves source order for tied timestamps across and within flows", "[traffic][replay]") {
    kns::SimulationEngine engine(topology());
    std::vector<std::pair<double, int>> seen;
    engine.setPacketObserver([&](const kns::Packet& p, auto, auto, auto, auto, auto) {
        if (p.hop_count == 0) seen.emplace_back(p.creation_time, p.serializedSize());
    });
    auto run = kns::traffic::TrafficRun::schedule(engine, plan());
    REQUIRE(engine.pendingEventCount() == 1);
    engine.run();
    REQUIRE(seen == std::vector<std::pair<double, int>>{{0, 125}, {0, 125}, {0, 250}, {0.01, 125}});
    const auto report = run->toJson(engine.now());
    REQUIRE(report["generated"] == 4); REQUIRE(report["delivered"] == 4);
    REQUIRE(report["flows"][0]["delivered_wire_bytes"] == 375);
    REQUIRE(report["flows"][1]["delivered_wire_bytes"] == 250);
    REQUIRE_FALSE(report["generation_pending"].get<bool>());
}
TEST_CASE("Replay interruption resumes its prefix and budget truncation remains explicit", "[traffic][replay][limits]") {
    for (const bool limited : {false, true}) {
        kns::SimulationEngine engine(topology());
        auto input = plan(); if (limited) input["max_packets"] = 2;
        auto run = kns::traffic::TrafficRun::schedule(engine, input);
        engine.runUntil(0, 1);
        REQUIRE(run->toJson(engine.now())["generated"] == 1);
        REQUIRE(run->toJson(engine.now())["generation_pending"].get<bool>());
        engine.run();
        const auto report = run->toJson(engine.now());
        REQUIRE(report["generated"] == (limited ? 2 : 4));
        REQUIRE(report["pending"] == 0);
        REQUIRE(report["generation_limit_reached"].get<bool>() == limited);
        REQUIRE(report["generation_pending"].get<bool>() == limited);
    }
}
TEST_CASE("Replay tie order controls which packet is rejected by a shared finite queue", "[traffic][replay][queue]") {
    kns::SimulationEngine engine(topology());
    engine.getTopology().getLinks()[0]->setQueueCapacity(2);
    auto run = kns::traffic::TrafficRun::schedule(engine, plan());
    engine.run();
    const auto report = run->toJson(engine.now());
    REQUIRE(report["generated"] == 4); REQUIRE(report["delivered"] == 3); REQUIRE(report["dropped"] == 1);
    REQUIRE(report["flows"][0]["delivered"] == 3);
    REQUIRE(report["flows"][1]["dropped"] == 1);
    REQUIRE(engine.getStats().queue_overflow_drops == 1);
}
TEST_CASE("Invalid replay plans schedule no partial source prefix", "[traffic][replay][validation]") {
    std::vector<Json> invalid;
    auto input = plan(); input["replay_events"][1]["at_s"] = -1; invalid.push_back(input);
    input = plan(); input["replay_events"][2]["flow_id"] = 9; invalid.push_back(input);
    input = plan(); input["replay_events"][3]["at_s"] = 1; invalid.push_back(input);
    input = plan(); input["replay_events"][3]["at_s"] = std::numeric_limits<double>::infinity(); invalid.push_back(input);
    input = plan(); input.erase("replay_events"); invalid.push_back(input);
    input = plan(); input["replay_events"] = Json::array(); invalid.push_back(input);
    input = plan(); input["replay_events"][2]["flow_id"] = 1; invalid.push_back(input); // unreferenced flow
    input = plan(); input["flows"][0]["schedule"] = {{"kind", "periodic"}, {"interval_s", 0.1}}; invalid.push_back(input);
    input = plan(); input["flows"][0]["application"] = {{"kind", "telemetry"}}; invalid.push_back(input);
    for (const auto& bad : invalid) {
        kns::SimulationEngine engine(topology());
        REQUIRE_THROWS(kns::traffic::TrafficRun::schedule(engine, bad));
        REQUIRE(engine.pendingEventCount() == 0);
    }
}
