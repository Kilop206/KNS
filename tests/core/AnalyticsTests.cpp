#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "analysis/LatencyStatistics.hpp"
#include "analysis/MetricsCollector.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "engine/experiments/FaultLab.hpp"
#include <limits>
#include <numeric>
#include <cmath>

namespace {
    using Json = nlohmann::json;
    Json config(double until = 0.02, double interval = 0.001) {
        return {{"schema_version", "1.0"}, {"until_s", until}, {"interval_s", interval}};
    }
}
TEST_CASE("Analytics latency statistics have known moments percentiles and histogram", "[analytics][statistics]") {
    kns::analysis::LatencyStatistics statistics(4);
    REQUIRE(statistics.toJson()["mean_s"].is_null());
    for (const double value : {1.0, 2.0, 3.0, 4.0}) statistics.add(value);
    auto report = statistics.toJson();
    REQUIRE(report["count"] == 4);
    REQUIRE(report["mean_s"].get<double>() == 2.5);
    REQUIRE(report["sample_stddev_s"].get<double>() == Catch::Approx(std::sqrt(5.0 / 3.0)));
    REQUIRE(report["median_s"].get<double>() == 2.5);
    REQUIRE(report["p95_s"].get<double>() == Catch::Approx(3.85));
    REQUIRE(report["p99_s"].get<double>() == Catch::Approx(3.97));
    statistics.add(5);
    report = statistics.toJson();
    REQUIRE(report["mean_s"] == 3);
    REQUIRE(report["median_s"] == 3.5);
    REQUIRE(report["discarded_samples"] == 1);
    REQUIRE(report["percentile_scope"] == "most_recent_retained_deliveries");
    const auto counts = report["histogram"]["counts"].get<std::vector<std::uint64_t>>();
    REQUIRE(std::accumulate(counts.begin(), counts.end(), std::uint64_t{0}) == 5);
    for (double invalid : {-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        REQUIRE_THROWS(statistics.add(invalid));
    REQUIRE(statistics.toJson()["count"] == 5);
}
TEST_CASE("Analytics moments remain stable for small variance on a large offset", "[analytics][statistics]") {
    kns::analysis::LatencyStatistics statistics;
    for (double value : {1e12 + 1, 1e12 + 2, 1e12 + 3}) statistics.add(value);
    REQUIRE(statistics.toJson()["mean_s"].get<double>() == 1e12 + 2);
    REQUIRE(statistics.toJson()["sample_stddev_s"].get<double>() == 1);
}
TEST_CASE("Analytics utilization clips serialization and excludes propagation", "[analytics][timing]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 1, 10);
    kns::SimulationEngine engine(topology);
    auto collector = kns::analysis::MetricsCollector::attach(engine, config(0.004));
    REQUIRE(engine.sendPacket(kns::Packet(0, 1, 0, 0, 125, 999), *engine.getTopology().getLinks()[0], 0));
    engine.run();
    const auto report = collector->toJson();
    REQUIRE(report["samples"][0]["links"][0]["utilization"]["ab"].is_null());
    REQUIRE(report["samples"][1]["links"][0]["utilization"]["ab"].get<double>() == Catch::Approx(1));
    REQUIRE(report["samples"][1]["links"][0]["utilization"]["ba"] == 0);
    REQUIRE(report["samples"][2]["links"][0]["utilization"]["ab"] == 0);
    REQUIRE(report["samples"][2]["links"][0]["queue_ab_or_shared"] == 1);
    REQUIRE(report["packets_delivered_in_scope"] == 0); // arrival at 0.011, beyond scope
    REQUIRE(report["data_latency"]["count"] == 0);
    REQUIRE_FALSE(report["recommendations"].empty());
}
TEST_CASE("Analytics full duplex and shared half duplex use distinct utilization slots", "[analytics][duplex]") {
    for (const auto mode : {kns::LinkMode::FULL_DUPLEX, kns::LinkMode::HALF_DUPLEX}) {
        kns::Topology topology(2);
        topology.addLink(0, 1, 1, 0, 0, mode);
        kns::SimulationEngine engine(topology);
        auto collector = kns::analysis::MetricsCollector::attach(engine, config(0.002, 0.002));
        auto link = engine.getTopology().getLinks()[0];
        REQUIRE(engine.sendPacket(kns::Packet(0, 1, 0, 0, 125, 999), *link, 0));
        REQUIRE(engine.sendPacket(kns::Packet(1, 0, 1, 0, 125, 999), *link, 0));
        engine.run();
        const auto utilization = collector->toJson()["samples"][1]["links"][0]["utilization"];
        if (mode == kns::LinkMode::FULL_DUPLEX) {
            REQUIRE(utilization["ab"].get<double>() == Catch::Approx(0.5));
            REQUIRE(utilization["ba"].get<double>() == Catch::Approx(0.5));
            REQUIRE(utilization["shared"].is_null());
        } else {
            REQUIRE(utilization["shared"].get<double>() == Catch::Approx(1));
            REQUIRE(utilization["ab"].is_null());
        }
    }
}
TEST_CASE("Analytics retention and grouping limits retain honest overflow counters", "[analytics][retention]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 1, 0);
    kns::SimulationEngine engine(topology);
    auto input = config(0.01);
    input.update({{"max_samples", 2}, {"latency_retention", 1}, {"max_groups", 1}, {"group_retention", 1}, {"max_reservations", 1}});
    auto collector = kns::analysis::MetricsCollector::attach(engine, input);
    const auto link = engine.getTopology().getLinks()[0];
    REQUIRE(engine.sendPacket(kns::Packet(0, 1, 0, 0, 125, 999), *link, 0));
    REQUIRE(engine.sendPacket(kns::Packet(1, 0, 1, 0, 125, 1000), *link, 0));
    engine.run();
    const auto report = collector->toJson();
    REQUIRE(report["samples"].size() == 2);
    REQUIRE(report["samples_seen"] == 11);
    REQUIRE(report["samples_discarded"] == 9);
    REQUIRE(report["data_latency"]["count"] == 2);
    REQUIRE(report["data_latency"]["discarded_samples"] == 1);
    REQUIRE(report["ungrouped_flow_deliveries"] == 1);
    REQUIRE(report["ungrouped_node_deliveries"] == 1);
    REQUIRE(report["reservation_records_discarded"] == 1);
    REQUIRE(report["samples"][1]["links"][0]["utilization"]["ab"].is_null());
    REQUIRE(report["link_summaries"][0]["max_sampled_directional_utilization"].is_null());
}
TEST_CASE("Analytics collection preserves seeded TCP simulation outputs", "[analytics][determinism][integration]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 10, 1, 0.02);
    kns::SimulationEngine observed(topology), plain(topology);
    auto collector = kns::analysis::MetricsCollector::attach(observed, config(0.1, 0.01));
    observed.startTCPConnection(0, 1); plain.startTCPConnection(0, 1);
    observed.run(); plain.run();
    REQUIRE(observed.getStats().packets_delivered == plain.getStats().packets_delivered);
    REQUIRE(observed.getStats().packets_lost == plain.getStats().packets_lost);
    REQUIRE(observed.getStats().total_latency == plain.getStats().total_latency);
    REQUIRE(observed.now() == plain.now());
    REQUIRE(collector->toJson()["data_latency"]["count"].get<int>() > 0);
}
TEST_CASE("Analytics config validation is bounded and attaches before execution only", "[analytics][validation]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 10, 1);
    for (auto input : {Json{{"schema_version", "2.0"}}, config(1, 0), config(1, 1e-12), config(-1, 1)}) {
        kns::SimulationEngine engine(topology);
        REQUIRE_THROWS(kns::analysis::MetricsCollector::attach(engine, input));
        REQUIRE_FALSE(engine.hasEvents());
    }
    kns::SimulationEngine engine(topology);
    auto collector = kns::analysis::MetricsCollector::attach(engine, config());
    const auto pending = engine.pendingEventCount();
    REQUIRE_THROWS(kns::analysis::MetricsCollector::attach(engine, config()));
    REQUIRE(engine.pendingEventCount() == pending);
}

TEST_CASE("Analytics records sampled fault state changes with virtual timestamps", "[analytics][faults]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 1, 1);
    kns::SimulationEngine engine(topology);
    auto collector = kns::analysis::MetricsCollector::attach(engine, config(0.004));
    Json plan{{"schema_version", "1.0"}, {"in_flight_policy", "complete_accepted_hops"},
        {"probes", Json::array()}, {"observe_at", Json::array()},
        {"events", {{{"at", 0.0005}, {"type", "link_down"}, {"link_index", 0}},
                    {{"at", 0.0025}, {"type", "link_up"}, {"link_index", 0}}}}};
    auto faults = kns::FaultLab::schedule(engine, plan);
    engine.run();
    const auto report = collector->toJson();
    REQUIRE(faults->changes.size() == 2);
    REQUIRE(report["link_changes_observed"] == 2);
    REQUIRE(report["link_changes"][0]["observed_at_s"].get<double>() == Catch::Approx(0.001));
    REQUIRE(report["link_changes"][0]["after"]["up"] == false);
    REQUIRE(report["link_changes"][1]["observed_at_s"].get<double>() == Catch::Approx(0.003));
}
TEST_CASE("Analytics counts serialization for modeled probabilistic link loss", "[analytics][loss]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 1, 10, 1);
    kns::SimulationEngine engine(topology);
    auto collector = kns::analysis::MetricsCollector::attach(engine, config(0.002));
    REQUIRE_FALSE(engine.sendPacket(kns::Packet(0, 1, 0, 0, 125, 999), *engine.getTopology().getLinks()[0], 0));
    engine.run();
    const auto report = collector->toJson();
    REQUIRE(engine.getStats().packets_lost == 1);
    REQUIRE(report["samples"][1]["links"][0]["utilization"]["ab"].get<double>() == Catch::Approx(1));
    REQUIRE(report["data_latency"]["count"] == 0);
}
