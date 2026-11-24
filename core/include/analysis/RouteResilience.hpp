#pragma once

#include "network/RouteTrace.hpp"
#include "network/Routing.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace kns {
    class SimulationEngine;
}

namespace kns::analysis {

    struct LinkOutagePreview {
        RouteHop failed_hop;
        RouteTrace alternative;
    };

    /// Directional N-1 analysis of the baseline forwarding path. Each advance()
    /// previews one cable outage, keeping GUI work incremental and results bounded.
    /// A reachable alternative is a configured route, not an SLA or delivery guarantee.
    class RouteResilience {
    public:
        static constexpr std::size_t MAX_FAILURES = 256;

        RouteResilience(const SimulationEngine& engine, int source, int destination,
                        std::size_t max_failures = 64);

        bool matches(const SimulationEngine& engine, int source, int destination) const noexcept;
        /// Returns true when a case was added; throws logic_error on stale input.
        bool advance(const SimulationEngine& engine);

        const RouteTrace& baseline() const noexcept {
            return baseline_;
        }

        const std::vector<LinkOutagePreview>& failures() const noexcept {
            return failures_;
        }

        std::size_t criticalCount() const noexcept;
        bool finished() const noexcept;
        /// False for unreachable/invalid baselines or a truncated analysis.
        bool complete() const noexcept;

    private:
        const SimulationEngine* engine_;
        int source_, destination_;
        std::uint64_t revision_;
        RoutingMetric metric_;
        std::size_t limit_;
        RouteTrace baseline_;
        std::vector<LinkOutagePreview> failures_;
    };
}
