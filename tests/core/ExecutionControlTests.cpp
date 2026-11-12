#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>
#include "engine/core/SimulationEngine.hpp"

namespace {
    class ActionEvent final : public kns::Event {
        std::function<void(kns::SimulationEngine&)> action_;
    public:
        ActionEvent(double at, std::function<void(kns::SimulationEngine&)> action)
            : Event(at), action_(std::move(action)) {}
        void execute(kns::SimulationEngine& engine) override { action_(engine); }
    };
    void schedule(kns::SimulationEngine& engine, double at,
                  std::function<void(kns::SimulationEngine&)> action = [](auto&) {}) {
        engine.schedule(std::make_unique<ActionEvent>(at, std::move(action)));
    }
    void repeat(kns::SimulationEngine& engine) { schedule(engine, engine.now(), repeat); }
}

TEST_CASE("Bounded execution includes horizon ties and preserves later events", "[core][execution]") {
    kns::SimulationEngine engine{kns::Topology(2)};
    std::vector<int> trace;
    schedule(engine, 1, [&](auto&) { trace.push_back(1); });
    schedule(engine, 1, [&](auto& e) {
        trace.push_back(2);
        schedule(e, 1, [&](auto&) { trace.push_back(3); });
    });
    schedule(engine, 3, [&](auto&) { trace.push_back(4); });
    auto result = engine.runUntil(1);
    REQUIRE(result.reason == kns::StopReason::TimeLimit);
    REQUIRE(result.events_processed == 3);
    REQUIRE(result.simulation_time_s == 1);
    REQUIRE(result.pending_events == 1);
    REQUIRE(trace == std::vector<int>{1, 2, 3});
    result = engine.runUntil(2);
    REQUIRE(result.events_processed == 0);
    REQUIRE(engine.now() == 2);
    engine.run();
    REQUIRE(trace == std::vector<int>{1, 2, 3, 4});
    REQUIRE(engine.executionStatistics().events_completed == 4);
}

TEST_CASE("Event budgets bound zero-time recurrence without losing work", "[core][execution]") {
    kns::SimulationEngine engine{kns::Topology(2)};
    repeat(engine);
    auto result = engine.runUntil(10, 0);
    REQUIRE(result.reason == kns::StopReason::EventBudget);
    REQUIRE(engine.now() == 0);
    result = engine.runUntil(10, 100);
    REQUIRE(result.reason == kns::StopReason::EventBudget);
    REQUIRE(result.events_processed == 100);
    REQUIRE(result.pending_events == 1);
    REQUIRE(engine.now() == 0);
    engine.cancel();
    REQUIRE(engine.runUntil(10).reason == kns::StopReason::Cancelled);
}

TEST_CASE("Pause step and resume preserve pending work and virtual clock", "[core][execution]") {
    kns::SimulationEngine engine{kns::Topology(2)};
    schedule(engine, 1, [](auto& e) { e.pause(); });
    schedule(engine, 2);
    schedule(engine, 3);
    REQUIRE(engine.runUntil(10).reason == kns::StopReason::Paused);
    REQUIRE(engine.now() == 1);
    REQUIRE_FALSE(engine.processEvent());
    engine.run();
    REQUIRE(engine.now() == 1);
    REQUIRE(engine.step());
    REQUIRE(engine.now() == 2);
    REQUIRE(engine.executionState() == kns::ExecutionState::Paused);
    REQUIRE(engine.runUntil(10).events_processed == 0);
    engine.resume();
    REQUIRE(engine.runUntil(10).reason == kns::StopReason::QueueEmpty);
    REQUIRE(engine.now() == 3);
    REQUIRE_FALSE(engine.step());
}

TEST_CASE("Cancellation is terminal and retains pending diagnostics", "[core][execution]") {
    kns::SimulationEngine engine{kns::Topology(2)};
    schedule(engine, 1, [](auto& e) {
        e.cancel();
        schedule(e, 2); // cancelling must let the active callback finish
    });
    const auto result = engine.runUntil(10);
    REQUIRE(result.reason == kns::StopReason::Cancelled);
    REQUIRE(result.events_processed == 1);
    REQUIRE(result.pending_events == 1);
    REQUIRE_FALSE(engine.step());
    REQUIRE_FALSE(engine.processEvent());
    engine.run();
    REQUIRE(engine.now() == 1);
    REQUIRE_THROWS_AS(engine.resume(), std::logic_error);
    REQUIRE_THROWS_AS(schedule(engine, 3), std::logic_error);
}

TEST_CASE("Failed callbacks propagate and cannot resume as successful work", "[core][execution]") {
    kns::SimulationEngine engine{kns::Topology(2)};
    schedule(engine, 1, [](auto&) { throw std::runtime_error("model failure"); });
    schedule(engine, 2);
    REQUIRE_THROWS_AS(engine.runUntil(10), std::runtime_error);
    REQUIRE(engine.executionState() == kns::ExecutionState::Failed);
    REQUIRE(engine.executionStatistics().events_dispatched == 1);
    REQUIRE(engine.executionStatistics().events_completed == 0);
    REQUIRE(engine.executionStatistics().events_failed == 1);
    REQUIRE(engine.pendingEventCount() == 1);
    REQUIRE(engine.runUntil(10).reason == kns::StopReason::Failed);
    engine.cancel();
    REQUIRE(engine.executionState() == kns::ExecutionState::Failed);
    REQUIRE_THROWS_AS(engine.resume(), std::logic_error);
}

TEST_CASE("Execution rejects recursive dispatch without skipping events", "[core][execution]") {
    kns::SimulationEngine engine{kns::Topology(2)};
    schedule(engine, 1, [](auto& e) {
        REQUIRE_THROWS_AS(e.run(), std::logic_error);
        REQUIRE_THROWS_AS(e.runUntil(2), std::logic_error);
        REQUIRE_THROWS_AS(e.step(), std::logic_error);
        REQUIRE_THROWS_AS(e.processEvent(), std::logic_error);
    });
    schedule(engine, 2);
    engine.run();
    REQUIRE(engine.executionStatistics().events_completed == 2);
    REQUIRE(engine.now() == 2);
}

TEST_CASE("Invalid horizons preserve state and drained runs do not invent duration", "[core][execution]") {
    kns::SimulationEngine engine{kns::Topology(2)};
    REQUIRE(engine.runUntil(100).reason == kns::StopReason::QueueEmpty);
    REQUIRE(engine.now() == 0);
    schedule(engine, 2);
    engine.runUntil(1);
    for (double time : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        REQUIRE_THROWS_AS(engine.runUntil(time), std::invalid_argument);
        REQUIRE(engine.now() == 1);
        REQUIRE(engine.pendingEventCount() == 1);
    }
}

TEST_CASE("Segmented TCP execution reproduces uninterrupted execution", "[integration][execution][determinism]") {
    kns::Topology topology(3);
    topology.addLink(0, 1, 10, 2, 0.02);
    topology.addLink(1, 2, 10, 2, 0.02);
    kns::SimulationEngine continuous(topology), segmented(topology);
    kns::RunConfig config;
    config.seed = 123;
    continuous.configureRun(config);
    segmented.configureRun(config);
    continuous.startTCPConnection(0, 2);
    segmented.startTCPConnection(0, 2);
    continuous.run();
    for (int iteration = 0; segmented.hasEvents() && iteration < 100000; ++iteration) {
        segmented.runUntil(segmented.now() + 0.01, 7);
    }
    REQUIRE_FALSE(segmented.hasEvents());
    REQUIRE(segmented.now() == continuous.now());
    REQUIRE(segmented.getStats().packets_sent == continuous.getStats().packets_sent);
    REQUIRE(segmented.getStats().packets_lost == continuous.getStats().packets_lost);
    REQUIRE(segmented.getStats().packets_delivered == continuous.getStats().packets_delivered);
    REQUIRE(segmented.getStats().total_latency == continuous.getStats().total_latency);
    REQUIRE(segmented.executionStatistics().events_completed == continuous.executionStatistics().events_completed);
}

TEST_CASE("Cancelled pending events do not count as processed callbacks", "[core][execution]") {
    kns::SimulationEngine engine{kns::Topology(2)};
    auto removed = std::make_unique<ActionEvent>(1, [](auto&) {
        FAIL("Cancelled event must never execute");
    });
    const auto id = removed->getId();
    engine.schedule(std::move(removed));
    schedule(engine, 2);
    REQUIRE(engine.cancelEvent(id));
    REQUIRE_FALSE(engine.cancelEvent(id));
    REQUIRE(engine.runUntil(3).events_processed == 1);
    REQUIRE(engine.executionStatistics().events_dispatched == 1);
    REQUIRE(engine.executionStatistics().events_completed == 1);
    REQUIRE(engine.executionStatistics().events_failed == 0);
}

TEST_CASE("Terminal cancellation cannot validate as a completed simulation", "[core][execution][validation]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 10, 1);
    kns::SimulationEngine engine(topology);
    engine.startTCPConnection(0, 1);
    engine.run();
    REQUIRE(engine.validateSimulation().passed());
    engine.cancel();
    REQUIRE_FALSE(engine.validateSimulation().passed());
}

TEST_CASE("Time limits and cancellation retain physically pending packets", "[integration][execution][timing]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 1.0, 10.0);
    kns::SimulationEngine engine(topology);
    const auto link = engine.getTopology().getLinks()[0];
    // 125 bytes / 1 Mbps = 1 ms serialization, plus 10 ms propagation.
    kns::Packet packet(0, 1, 0, 0, 125, 999);
    REQUIRE(engine.sendPacket(packet, *link, 0));
    const auto result = engine.runUntil(0.005);
    REQUIRE(result.reason == kns::StopReason::TimeLimit);
    REQUIRE(result.events_processed == 0);
    REQUIRE(result.pending_events == 1);
    REQUIRE(engine.getStats().packets_delivered == 0);
    REQUIRE(engine.getPacketsInTransit().size() == 1);
    REQUIRE(link->getQueueSize() == 1);
    engine.cancel();
    engine.run();
    REQUIRE(engine.now() == 0.005);
    REQUIRE(engine.getStats().packets_sent == 1);
    REQUIRE(engine.getStats().packets_delivered == 0);
    REQUIRE(engine.getStats().packets_lost == 0);
    REQUIRE(engine.getPacketsInTransit().size() == 1);
    REQUIRE(engine.pendingEventCount() == 1);
}
