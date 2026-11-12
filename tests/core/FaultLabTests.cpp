#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/experiments/FaultLab.hpp"
#include "engine/core/SimulationEngine.hpp"

namespace {
    using Json = nlohmann::json;
    Json plan() {
        return {{"schema_version", "1.0"}, {"in_flight_policy", "complete_accepted_hops"},
            {"events", Json::array()}, {"probes", {{{"source", 0}, {"destination", 1}}}},
            {"observe_at", {0, 1, 2, 3, 4}}};
    }
    Json state(double at, const char* type, int link = 0) {
        return {{"at", at}, {"type", type}, {"link_index", link}};
    }
}

TEST_CASE("Fault Lab reports connectivity outage separately from service recovery", "[fault-lab][integration]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 10, 1);
    kns::SimulationEngine engine(topology);
    auto input = plan();
    input["events"] = {state(1, "link_down"), state(3, "link_up")};
    auto report = kns::FaultLab::schedule(engine, input);
    engine.run();
    const auto output = report->toJson();
    REQUIRE(output["changes"].size() == 2);
    REQUIRE(output["observations"][0]["probes"][0]["status"] == "reachable");
    REQUIRE(output["observations"][1]["probes"][0]["status"] == "unreachable");
    REQUIRE(output["sampled_connectivity_outages"][0]["duration_s"] == 2);
    REQUIRE(output["service_recovery_s"].is_null());
    REQUIRE(output["windows"][0]["mean_data_latency_s"].is_null());
    REQUIRE(output["windows"][0]["throughput_pps"] == 0);
}

TEST_CASE("Fault Lab takes alternate routes and identifies parallel links by original index", "[fault-lab][routing]") {
    kns::Topology topology(3);
    topology.addLink(0, 1, 10, 1);
    topology.addLink(0, 1, 10, 5);
    topology.addLink(0, 2, 10, 2);
    topology.addLink(2, 1, 10, 2);
    kns::SimulationEngine engine(topology);
    auto input = plan();
    input["events"] = {state(1, "link_down"), state(3, "link_up")};
    auto report = kns::FaultLab::schedule(engine, input);
    engine.run();
    const auto& samples = report->observations;
    REQUIRE(samples[0]["probes"][0]["hops"][0]["link_index"] == 0);
    REQUIRE(samples[1]["probes"][0]["hops"].size() == 2);
    REQUIRE(samples[1]["probes"][0]["hops"][0]["link_index"] == 2);
    REQUIRE(samples[1]["links"][1]["up"] == true);
    REQUIRE(samples[3]["probes"][0]["hops"][0]["link_index"] == 0);
    REQUIRE(report->toJson()["sampled_connectivity_outages"].empty());
    REQUIRE(report->toJson()["sampled_link_outages"][0]["duration_s"] == 2);
}

TEST_CASE("Fault Lab preserves accepted and queued hop timing across failure", "[fault-lab][timing]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 1, 10);
    kns::SimulationEngine engine(topology);
    const auto link = engine.getTopology().getLinks()[0];
    kns::Packet packet(0, 1, 0, 0, 125, 999);
    REQUIRE(engine.sendPacket(packet, *link, 0));
    REQUIRE(engine.sendPacket(packet, *link, 0));
    auto input = plan();
    input["observe_at"] = {0, 0.005, 0.015, 0.03};
    input["events"] = {state(0.0005, "link_down"),
        Json{{"at", 0.0005}, {"type", "bandwidth_mbps"}, {"link_index", 0}, {"value", 0.1}},
        Json{{"at", 0.0005}, {"type", "delay_ms"}, {"link_index", 0}, {"value", 1000}}, state(0.02, "link_up")};
    auto report = kns::FaultLab::schedule(engine, input);
    engine.runUntil(0.005);
    REQUIRE(engine.getStats().packets_delivered == 0);
    engine.runUntil(0.015);
    REQUIRE(engine.getStats().packets_delivered == 2);
    REQUIRE(engine.getStats().packets_lost == 0);
    REQUIRE(link->getQueueSize() == 0);
    engine.run();
    const auto output = report->toJson();
    int delivered = 0;
    for (const auto& window : output["windows"]) delivered += window["packets_delivered"].get<int>();
    REQUIRE(delivered == 2);
}

TEST_CASE("Fault Lab applies and explicitly restores bandwidth delay and loss", "[fault-lab][parameters]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 10, 1);
    kns::SimulationEngine engine(topology);
    auto input = plan();
    input["events"] = {
        {{"at", 1}, {"type", "bandwidth_mbps"}, {"link_index", 0}, {"value", 2}},
        {{"at", 1}, {"type", "delay_ms"}, {"link_index", 0}, {"value", 20}},
        {{"at", 1}, {"type", "loss_probability"}, {"link_index", 0}, {"value", 1}},
        {{"at", 3}, {"type", "bandwidth_mbps"}, {"link_index", 0}, {"value", 10}},
        {{"at", 3}, {"type", "delay_ms"}, {"link_index", 0}, {"value", 1}},
        {{"at", 3}, {"type", "loss_probability"}, {"link_index", 0}, {"value", 0}}};
    auto report = kns::FaultLab::schedule(engine, input);
    engine.runUntil(2);
    const auto link = engine.getTopology().getLinks()[0];
    REQUIRE(link->getBandwidthMbps() == 2);
    REQUIRE(link->getDelayMs() == 20);
    kns::Packet packet(0, 1, 0, engine.now(), 125, 999);
    REQUIRE_FALSE(engine.sendPacket(packet, *link, engine.now()));
    REQUIRE(engine.getStats().packets_lost == 1);
    engine.run();
    REQUIRE(link->getBandwidthMbps() == 10);
    REQUIRE(link->getDelayMs() == 1);
    REQUIRE(link->getLossProb() == 0);
    REQUIRE(report->changes.size() == 6);
    REQUIRE(report->observations[3]["packets_lost"] == 1);
}

TEST_CASE("Fault Lab rejects the whole invalid plan before scheduling", "[fault-lab][validation]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 10, 1);
    for (auto event : {state(1, "unknown"), state(-1, "link_down"), state(1, "link_down", 4),
                       Json{{"at", 1}, {"type", "loss_probability"}, {"link_index", 0}, {"value", 2}}}) {
        kns::SimulationEngine engine(topology);
        auto input = plan();
        input["events"] = {state(0.5, "link_down"), event};
        REQUIRE_THROWS(kns::FaultLab::schedule(engine, input));
        REQUIRE_FALSE(engine.hasEvents());
        REQUIRE(engine.getTopology().getLinks()[0]->isUp());
    }
}

TEST_CASE("Fault Lab preserves file order at ties and censors open outages", "[fault-lab][determinism]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 10, 1);
    kns::SimulationEngine engine(topology);
    auto input = plan();
    input["events"] = {state(1, "link_down"), state(1, "link_up"), state(2, "link_down")};
    auto report = kns::FaultLab::schedule(engine, input);
    engine.run();
    REQUIRE(report->observations[1]["links"][0]["up"] == true);
    const auto outage = report->toJson()["sampled_connectivity_outages"][0];
    REQUIRE(outage["from_s"] == 2);
    REQUIRE(outage["censored"] == true);
    REQUIRE(outage["duration_s"].is_null());
}

TEST_CASE("Fault Lab flapping produces repeated observed interruption intervals", "[fault-lab][flapping]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 10, 1);
    kns::SimulationEngine engine(topology);
    auto input = plan();
    input["events"] = {state(1, "link_down"), state(2, "link_up"), state(3, "link_down"), state(4, "link_up")};
    auto report = kns::FaultLab::schedule(engine, input);
    engine.run();
    const auto outages = report->toJson()["sampled_connectivity_outages"];
    REQUIRE(outages.size() == 2);
    REQUIRE(outages[0]["duration_s"] == 1);
    REQUIRE(outages[1]["duration_s"] == 1);
}

TEST_CASE("Fault Lab reports a removed scheduled target as execution failure", "[fault-lab][validation]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 10, 1);
    kns::SimulationEngine engine(topology);
    auto input = plan();
    input["events"] = {state(1, "link_down")};
    auto report = kns::FaultLab::schedule(engine, input);
    engine.deleteLinkById(engine.getTopology().getLinks()[0]->getId());
    REQUIRE_THROWS_AS(engine.run(), std::runtime_error);
    REQUIRE(engine.executionState() == kns::ExecutionState::Failed);
    REQUIRE(report->changes.empty());
}
