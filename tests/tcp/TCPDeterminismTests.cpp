#include <catch2/catch_test_macros.hpp>
#include "engine/core/EventQueue.hpp"
#include "engine/core/Event.hpp"
#include <memory>
#include <vector>

namespace kns {

    class DetermTestEvent : public Event {
    public:
        DetermTestEvent(double timestamp) : Event(timestamp) {}
        void execute(SimulationEngine&) override {}
    };

    TEST_CASE("EventQueue guarantees absolute determinism for concurrent events", "[core][determinism]")
    {
        // Execute the same scheduling sequence multiple times and verify identical outcome
        std::vector<std::uint64_t> first_run_order;
        std::vector<std::uint64_t> second_run_order;

        {
            EventQueue queue;
            // Schedule 50 events all at timestamp 42.0
            for (int i = 0; i < 50; ++i) {
                queue.schedule(std::make_unique<DetermTestEvent>(42.0));
            }

            while (queue.hasEvents()) {
                first_run_order.push_back(queue.next()->getId());
            }
        }

        {
            EventQueue queue;
            // Schedule 50 events all at timestamp 42.0
            for (int i = 0; i < 50; ++i) {
                queue.schedule(std::make_unique<DetermTestEvent>(42.0));
            }

            while (queue.hasEvents()) {
                second_run_order.push_back(queue.next()->getId());
            }
        }

        REQUIRE(first_run_order.size() == 50);
        REQUIRE(second_run_order.size() == 50);

        // Verify that the ordering within each run is strictly ascending by ID
        for (size_t i = 1; i < first_run_order.size(); ++i) {
            REQUIRE(first_run_order[i - 1] < first_run_order[i]);
            REQUIRE(second_run_order[i - 1] < second_run_order[i]);
        }

        // Verify both runs are identical in order relative to their start IDs
        for (size_t i = 0; i < 50; ++i) {
            // The difference between consecutive IDs should be identical
            if (i > 0) {
                REQUIRE((first_run_order[i] - first_run_order[i-1]) == (second_run_order[i] - second_run_order[i-1]));
            }
        }
    }

}
