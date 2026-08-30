#pragma once

#include "engine/core/Event.hpp"

namespace kns {

    /// Scheduled event that brings a link between two nodes up or down at a
    /// specific simulation time. Use this to model link failures and recoveries
    /// during a running simulation.
    ///
    /// Example usage (schedule a failure at t=1.0, recovery at t=3.0):
    ///   engine.schedule(std::make_unique<LinkFailureEvent>(1.0, 0, 1, false));
    ///   engine.schedule(std::make_unique<LinkFailureEvent>(3.0, 0, 1, true));
    class LinkFailureEvent : public Event {
        public:
            /// @param timestamp  Simulation time at which the state change occurs.
            /// @param node_a     One endpoint of the link.
            /// @param node_b     Other endpoint of the link.
            /// @param up         true = bring link up (recovery), false = bring link down (failure).
            LinkFailureEvent(double timestamp, int node_a, int node_b, bool up);

            void execute(SimulationEngine& engine) override;
            const char* getName() const noexcept override { return "LinkFailureEvent"; }

        private:
            int node_a_;
            int node_b_;
            bool up_;
    };

}
