#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <memory>
#include <stdexcept>
#include "engine/core/Event.hpp"
#include "engine/core/SimulationClock.hpp"
#include "engine/core/SimulationEngine.hpp"

namespace {
    class ClockEvent final : public kns::Event {
    public:
        explicit ClockEvent(double time) : Event(time) {}
        void execute(kns::SimulationEngine&) override {}
    };
}

TEST_CASE("Event construction rejects invalid timestamps", "[core][time]")
{
    for (double time : {-1.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        CAPTURE(time);
        REQUIRE_THROWS_AS(ClockEvent(time), std::invalid_argument);
    }
    REQUIRE_NOTHROW(ClockEvent(0.0));
}

TEST_CASE("Clock rejects invalid updates and overflow without mutation", "[core][time]")
{
    kns::SimulationClock clock;
    clock.setTime(10.0);
    for (double value : {-1.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        REQUIRE_THROWS_AS(clock.setTime(value), std::invalid_argument);
        REQUIRE_THROWS_AS(clock.tick(value), std::invalid_argument);
        REQUIRE(clock.now() == 10.0);
    }
    REQUIRE_THROWS_AS(clock.setTime(5.0), std::invalid_argument);
    clock.tick(0.0);
    REQUIRE(clock.now() == 10.0);
    clock.setTime(std::numeric_limits<double>::max());
    REQUIRE_THROWS_AS(clock.tick(std::numeric_limits<double>::max()), std::invalid_argument);
    REQUIRE(clock.now() == std::numeric_limits<double>::max());
}

TEST_CASE("Engine rejects backward scheduling and skipping queued events", "[core][time]")
{
    kns::SimulationEngine engine(kns::Topology(2));
    engine.advanceTime(10.0);
    engine.schedule(std::make_unique<ClockEvent>(12.0));
    REQUIRE_THROWS_AS(engine.schedule(std::make_unique<ClockEvent>(5.0)), std::invalid_argument);
    REQUIRE_THROWS_AS(engine.schedule(nullptr), std::invalid_argument);
    REQUIRE_THROWS_AS(engine.advanceTime(13.0), std::invalid_argument);
    REQUIRE_THROWS_AS(engine.advanceTime(5.0), std::invalid_argument);
    REQUIRE_THROWS_AS(engine.advanceTime(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    REQUIRE(engine.now() == 10.0);
    REQUIRE(engine.peekNextEventTime() == 12.0);
    engine.schedule(std::make_unique<ClockEvent>(10.0));
    REQUIRE(engine.processEvent());
    REQUIRE(engine.now() == 10.0);
    engine.run();
    REQUIRE(engine.now() == 12.0);
    REQUIRE_FALSE(engine.hasEvents());
}

TEST_CASE("Invalid packet times do not reserve links or change statistics", "[core][time]")
{
    kns::Topology topology(2);
    auto link = topology.addLinkPtr(0, 1, 10.0, 1.0);
    kns::SimulationEngine engine(topology);
    engine.advanceTime(10.0);
    link = engine.getTopology().getLinks()[0];
    kns::Packet packet(0, 1, 0, 10.0, 100, 0);
    for (double time : {5.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity()}) {
        REQUIRE_THROWS_AS(engine.sendPacket(packet, *link, time), std::invalid_argument);
    }
    link->setBandwidthMbps(std::numeric_limits<double>::denorm_min());
    REQUIRE_THROWS_AS(engine.sendPacket(packet, *link, 10.0), std::invalid_argument);
    REQUIRE(engine.getStats().packets_sent == 0);
    REQUIRE(link->getQueueSize() == 0);
    REQUIRE(link->getNextAvailableTime(0, 1, 10.0) == 10.0);
    REQUIRE(engine.getPacketsInTransit().empty());
    REQUIRE_FALSE(engine.hasEvents());
}
