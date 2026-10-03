#include <catch2/catch_test_macros.hpp>
#include "engine/core/EventQueue.hpp"
#include "engine/core/Event.hpp"
#include <memory>

namespace kns {

    class TestEvent : public Event {
    public:
        TestEvent(double timestamp) : Event(timestamp) {}
        void execute(SimulationEngine&) override {}
    };

    TEST_CASE("EventQueue schedules and retrieves events in correct order", "[core][event-queue]")
    {
        EventQueue queue;

        REQUIRE_FALSE(queue.hasEvents());
        REQUIRE(queue.size() == 0);

        // Schedule out of order timestamps
        queue.schedule(std::make_unique<TestEvent>(10.0));
        queue.schedule(std::make_unique<TestEvent>(5.0));
        queue.schedule(std::make_unique<TestEvent>(20.0));

        REQUIRE(queue.hasEvents());
        REQUIRE(queue.size() == 3);
        REQUIRE(queue.peekTimestamp() == 5.0);

        // Retrieve next and assert order
        auto e1 = queue.next();
        REQUIRE(e1 != nullptr);
        REQUIRE(e1->getTimestamp() == 5.0);
        REQUIRE(queue.size() == 2);

        auto e2 = queue.next();
        REQUIRE(e2 != nullptr);
        REQUIRE(e2->getTimestamp() == 10.0);

        auto e3 = queue.next();
        REQUIRE(e3 != nullptr);
        REQUIRE(e3->getTimestamp() == 20.0);

        REQUIRE_FALSE(queue.hasEvents());
        REQUIRE(queue.size() == 0);
        REQUIRE(queue.next() == nullptr);
    }

    TEST_CASE("EventQueue resolves timestamp ties using event ID (FIFO ordering)", "[core][event-queue]")
    {
        EventQueue queue;

        // Schedule events with same timestamp
        queue.schedule(std::make_unique<TestEvent>(10.0));
        queue.schedule(std::make_unique<TestEvent>(10.0));
        queue.schedule(std::make_unique<TestEvent>(10.0));

        REQUIRE(queue.size() == 3);

        auto e1 = queue.next();
        auto e2 = queue.next();
        auto e3 = queue.next();

        REQUIRE(e1 != nullptr);
        REQUIRE(e2 != nullptr);
        REQUIRE(e3 != nullptr);

        // Check IDs are in ascending order (lower ID has higher priority)
        REQUIRE(e1->getId() < e2->getId());
        REQUIRE(e2->getId() < e3->getId());
    }

    TEST_CASE("EventQueue clear resets queue state", "[core][event-queue]")
    {
        EventQueue queue;

        queue.schedule(std::make_unique<TestEvent>(1.0));
        queue.schedule(std::make_unique<TestEvent>(2.0));
        REQUIRE(queue.size() == 2);

        queue.clear();
        REQUIRE(queue.size() == 0);
        REQUIRE_FALSE(queue.hasEvents());
        REQUIRE(queue.next() == nullptr);
    }

}
