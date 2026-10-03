#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"

namespace kns {

    /// Scheduled event that brings a link up or down at a specific simulation
    /// time. Endpoint targeting is supported only when that pair identifies a
    /// unique link; use the link-id constructor for parallel links.
    ///
    /// Example usage (schedule a failure at t=1.0, recovery at t=3.0):
    ///   engine.schedule(std::make_unique<LinkFailureEvent>(1.0, 0, 1, false));
    ///   engine.schedule(std::make_unique<LinkFailureEvent>(3.0, 0, 1, true));
    ///   engine.schedule(std::make_unique<LinkFailureEvent>(1.0, link_id, false));
    class LinkFailureEvent : public Event {
        public:
            /// @param timestamp  Simulation time at which the state change occurs.
            /// @param node_a     One endpoint of the link.
            /// @param node_b     Other endpoint of the link.
            /// @param up         true = bring link up (recovery), false = bring link down (failure).
            LinkFailureEvent(double timestamp, int node_a, int node_b, bool up);

            /// Target one exact link, including when its endpoints have other
            /// parallel links.
            LinkFailureEvent(double timestamp, std::uint64_t link_id, bool up);

            void execute(SimulationEngine& engine) override;
            const char* getName() const noexcept override { return "LinkFailureEvent"; }

        private:
            int node_a_;
            int node_b_;
            std::uint64_t link_id_;
            bool target_by_id_;
            bool up_;
    };

}
