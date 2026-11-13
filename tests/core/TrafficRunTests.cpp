#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "traffic/TrafficRun.hpp"
#include "analysis/MetricsCollector.hpp"
#include "engine/core/SimulationEngine.hpp"
#include <limits>

namespace {
using Json = nlohmann::json;
Json flow(Json schedule = {{"kind", "periodic"}, {"interval_s", 0.01}}) {
    return {{"id", 1}, {"source", 0}, {"destination", 1}, {"start_s", 0}, {"end_s", 0.03},
        {"wire_bytes", 125}, {"schedule", schedule}};
}
Json plan(Json item = flow()) {
    return {{"schema_version", "1.0"}, {"seed", 42}, {"max_packets", 100000}, {"flows", Json::array({item})}};
}
kns::Topology topology() {
    kns::Topology t(3);
    t.addLink(0, 1, 1, 1); t.addLink(1, 2, 1, 1);
    return t;
}
}
TEST_CASE("Traffic periodic and CBR use actual serialization and scoped Analytics", "[traffic][analytics][timing]") {
    for (const auto& schedule : {Json{{"kind", "periodic"}, {"interval_s", 0.01}}, Json{{"kind", "cbr"}, {"rate_bps", 100000}}}) {
        kns::SimulationEngine engine(topology());
        auto collector = kns::analysis::MetricsCollector::attach(engine, {{"schema_version", "1.0"}, {"until_s", 0.04}, {"interval_s", 0.005}});
        auto run = kns::traffic::TrafficRun::schedule(engine, plan(flow(schedule)));
        engine.run();
        const auto report = run->toJson(engine.now());
        REQUIRE(report["generated"] == 3); REQUIRE(report["delivered"] == 3);
        REQUIRE(report["dropped"] == 0); REQUIRE(report["pending"] == 0);
        REQUIRE(report["flows"][0]["delivered_wire_bytes"] == 375);
        REQUIRE_FALSE(report["generation_pending"].get<bool>());
        REQUIRE_FALSE(report["generation_limit_reached"].get<bool>());
        const auto metrics = collector->toJson();
        REQUIRE(metrics["data_latency"]["mean_s"].get<double>() == Catch::Approx(0.002));
        REQUIRE(metrics["tcp_contiguous_payload_bytes"] == 0);
        REQUIRE(metrics["flow_groups"][0]["key"] == "traffic:1:0:1");
        REQUIRE(engine.getTCPSessions().empty());
    }
}
TEST_CASE("Traffic burst congestion conserves generated delivered and discarded packets", "[traffic][queue][conservation]") {
    auto t = topology();
    kns::SimulationEngine engine(t);
    engine.getTopology().getLinks()[0]->setQueueCapacity(2);
    auto item = flow({{"kind", "burst"}, {"burst_packets", 3}, {"burst_interval_s", 0.01}, {"packet_interval_s", 0}});
    item["end_s"] = 0.02;
    auto run = kns::traffic::TrafficRun::schedule(engine, plan(item));
    engine.run();
    const auto report = run->toJson(engine.now());
    REQUIRE(report["generated"] == 6); REQUIRE(report["delivered"] == 4);
    REQUIRE(report["dropped"] == 2); REQUIRE(report["pending"] == 0);
    REQUIRE(engine.getStats().queue_overflow_drops == 2);
}
TEST_CASE("Traffic reports rejection at source intermediate hops and TTL exhaustion", "[traffic][loss][ttl]") {
    for (const int failure : {0, 1, 2}) {
        kns::SimulationEngine engine(topology());
        auto item = flow(); item["destination"] = 2;
        if (failure == 0) engine.getTopology().getLinks()[0]->setUp(false);
        if (failure == 1) engine.getTopology().getLinks()[1]->setLossProb(1);
        if (failure == 2) item["ttl_hops"] = 1;
        auto run = kns::traffic::TrafficRun::schedule(engine, plan(item));
        engine.run();
        const auto report = run->toJson(engine.now());
        REQUIRE(report["generated"] == 3); REQUIRE(report["dropped"] == 3);
        REQUIRE(report["delivered"] == 0); REQUIRE(report["pending"] == 0);
        REQUIRE(engine.getStats().packets_lost == 3);
    }
}
TEST_CASE("Traffic distinguishes pending packets cancellation and generation budget", "[traffic][limits]") {
    kns::SimulationEngine engine(topology());
    auto run = kns::traffic::TrafficRun::schedule(engine, plan());
    engine.runUntil(0.0005);
    auto report = run->toJson(engine.now());
    REQUIRE(report["generated"] == 1); REQUIRE(report["pending"] == 1);
    REQUIRE(report["generation_pending"].get<bool>());
    engine.cancel();
    REQUIRE(run->toJson(engine.now())["pending"] == 1);
    kns::SimulationEngine limited(topology());
    auto input = plan(); input["max_packets"] = 2;
    auto capped = kns::traffic::TrafficRun::schedule(limited, input);
    limited.run();
    report = capped->toJson(limited.now());
    REQUIRE(report["generated"] == 2); REQUIRE(report["delivered"] == 2);
    REQUIRE(report["generation_limit_reached"].get<bool>());
    REQUIRE_FALSE(report["generation_pending"].get<bool>());
}
TEST_CASE("Traffic deadlines classify delivery without discarding late packets", "[traffic][deadline]") {
    for (const double deadline : {0.001, 0.003}) {
        kns::SimulationEngine engine(topology());
        auto item = flow(); item["deadline_s"] = deadline;
        auto run = kns::traffic::TrafficRun::schedule(engine, plan(item));
        engine.run();
        const auto result = run->toJson(engine.now())["flows"][0];
        REQUIRE(result["delivered_on_time"] == (deadline > 0.002 ? 3 : 0));
        REQUIRE(result["delivered_late"] == (deadline < 0.002 ? 3 : 0));
        REQUIRE(result["delivered"] == 3);
    }
}
TEST_CASE("Traffic stochastic streams replay independently of other flows", "[traffic][random][determinism]") {
    const auto collect = [](const char* kind, bool extra, std::uint64_t seed) {
        kns::SimulationEngine engine(topology());
        std::vector<double> times;
        engine.setPacketObserver([&](const kns::Packet& packet, auto, auto, auto, auto, auto) {
            if (packet.workload && packet.workload->flow_id == 1 && packet.hop_count == 0) times.push_back(packet.creation_time);
        });
        auto item = flow(std::string(kind) == "poisson" ? Json{{"kind", kind}, {"rate_pps", 1000}} :
            Json{{"kind", kind}, {"min_interval_s", 0.0005}, {"max_interval_s", 0.0015}});
        item["end_s"] = 0.1;
        auto input = plan(item); input["seed"] = seed;
        if (extra) { auto other = flow(); other["id"] = 9; other["source"] = 1; other["destination"] = 0; input["flows"].push_back(other); }
        auto run = kns::traffic::TrafficRun::schedule(engine, input);
        engine.run();
        REQUIRE(run->toJson(engine.now())["pending"] == 0);
        return times;
    };
    for (const auto* kind : {"poisson", "uniform"}) {
        const auto original = collect(kind, false, 42);
        REQUIRE(original.size() > 20);
        REQUIRE(original == collect(kind, false, 42));
        REQUIRE(original == collect(kind, true, 42));
        REQUIRE(original != collect(kind, false, 43));
        for (std::size_t i = 1; i < original.size(); ++i) REQUIRE(original[i] > original[i - 1]);
    }
}
TEST_CASE("Traffic validates whole plans before scheduling any events", "[traffic][validation]") {
    std::vector<Json> invalid;
    auto value = plan(); value["flows"][0]["schedule"]["interval_s"] = 0; invalid.push_back(value);
    value = plan(); value["flows"][0]["destination"] = 99; invalid.push_back(value);
    value = plan(); value["flows"].push_back(flow()); invalid.push_back(value);
    value = plan(); value["seed"] = -1; invalid.push_back(value);
    value = plan(); value["flows"][0]["priority"] = 8; invalid.push_back(value);
    value = plan(); value["flows"][0]["schedule"] = {{"kind", "custom"}, {"offsets_s", {0.01, 0}}}; invalid.push_back(value);
    value = plan(); value["flows"][0]["schedule"] = {{"kind", "burst"}, {"burst_packets", 2}, {"burst_interval_s", 1}, {"packet_interval_s", 1}}; invalid.push_back(value);
    value = plan(); value["flows"][0]["schedule"]["interval_s"] = std::numeric_limits<double>::infinity(); invalid.push_back(value);
    for (const auto& input : invalid) {
        kns::SimulationEngine engine(topology());
        REQUIRE_THROWS(kns::traffic::TrafficRun::schedule(engine, input));
        REQUIRE(engine.pendingEventCount() == 0);
    }
    kns::SimulationEngine engine(topology());
    auto run = kns::traffic::TrafficRun::schedule(engine, plan());
    const auto pending = engine.pendingEventCount();
    REQUIRE_THROWS(kns::traffic::TrafficRun::schedule(engine, plan()));
    REQUIRE(engine.pendingEventCount() == pending);
    kns::SimulationEngine cancelled(topology());
    cancelled.cancel();
    REQUIRE_THROWS(kns::traffic::TrafficRun::schedule(cancelled, plan()));
    REQUIRE(cancelled.pendingEventCount() == 0);
}
