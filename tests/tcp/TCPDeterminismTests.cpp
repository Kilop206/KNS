#include <catch2/catch_test_macros.hpp>
#include "engine/core/EventQueue.hpp"
#include "engine/core/Event.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/Topology.hpp"
#include "network/Packet.hpp"
#include "enums/LinkMode.hpp"
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace kns;

class DetermTestEvent : public Event {
public:
    DetermTestEvent(double timestamp) : Event(timestamp) {}
    void execute(SimulationEngine&) override {}
};

TEST_CASE("EventQueue guarantees absolute determinism for concurrent events", "[core][determinism]")
{
    std::vector<std::uint64_t> first_run_order;
    std::vector<std::uint64_t> second_run_order;

    {
        EventQueue queue;
        for (int i = 0; i < 50; ++i) {
            queue.schedule(std::make_unique<DetermTestEvent>(42.0));
        }

        while (queue.hasEvents()) {
            first_run_order.push_back(queue.next()->getId());
        }
    }

    {
        EventQueue queue;
        for (int i = 0; i < 50; ++i) {
            queue.schedule(std::make_unique<DetermTestEvent>(42.0));
        }

        while (queue.hasEvents()) {
            second_run_order.push_back(queue.next()->getId());
        }
    }

    REQUIRE(first_run_order.size() == 50);
    REQUIRE(second_run_order.size() == 50);

    for (size_t i = 1; i < first_run_order.size(); ++i) {
        REQUIRE(first_run_order[i - 1] < first_run_order[i]);
        REQUIRE(second_run_order[i - 1] < second_run_order[i]);
    }

    for (size_t i = 0; i < 50; ++i) {
        if (i > 0) {
            REQUIRE((first_run_order[i] - first_run_order[i-1]) == (second_run_order[i] - second_run_order[i-1]));
        }
    }
}

TEST_CASE("End-to-end simulation determinism across multiple runs", "[core][determinism]")
{
    auto run_simulation = [](unsigned int seed) {
        std::srand(seed);
        Topology topo(4);
        topo.addLink(0, 1, 100.0, 5.0, 0.0, LinkMode::FULL_DUPLEX);
        topo.addLink(1, 2, 100.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);
        topo.addLink(2, 3, 100.0, 8.0, 0.0, LinkMode::FULL_DUPLEX);
        topo.addLink(3, 0, 100.0, 6.0, 0.0, LinkMode::FULL_DUPLEX);

        SimulationEngine engine(topo);
        std::vector<std::string> event_trace;

        engine.setPacketObserver([&](const Packet&, uint64_t, int from, int to, double dep, double arr) {
            event_trace.push_back(std::to_string(dep) + ":" + std::to_string(arr) + ":" + std::to_string(from) + "->" + std::to_string(to));
        });

        engine.startTCPConnection(0, 2);
        engine.run();

        return std::make_pair(event_trace, engine.getStats().packets_delivered);
    };

    auto [trace1, packets1] = run_simulation(12345);
    auto [trace2, packets2] = run_simulation(12345);

    REQUIRE(packets1 > 0);
    REQUIRE(packets1 == packets2);
    REQUIRE(trace1 == trace2);
}