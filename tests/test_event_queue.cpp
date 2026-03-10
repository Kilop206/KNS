#include <catch2/catch_test_macros.hpp>

#include "core/EventQueue.h"
#include "core/Event.h"

class TestEvent : public core::Event {
public:
    TestEvent(std::uint64_t t) : Event(t) {}

    void execute(core::SimulationEngine&) override {}
};

TEST_CASE("EventQueue schedules events") {
    core::EventQueue queue;

    queue.schedule(std::make_unique<TestEvent>(10));
    queue.schedule(std::make_unique<TestEvent>(5));

    REQUIRE(queue.size() == 2);
}

TEST_CASE("EventQueue pops earliest event first") {
    core::EventQueue queue;

    queue.schedule(std::make_unique<TestEvent>(10));
    queue.schedule(std::make_unique<TestEvent>(5));

    auto first = queue.next();
    auto second = queue.next();

    REQUIRE(first->getTimestamp() <= second->getTimestamp());
}