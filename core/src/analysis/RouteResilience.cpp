#include "analysis/RouteResilience.hpp"
#include "engine/core/SimulationEngine.hpp"
#include <algorithm>
#include <stdexcept>

namespace kns::analysis {

    RouteResilience::RouteResilience(const SimulationEngine& engine, int source, int destination,
                                    std::size_t max_failures)
        : engine_(&engine), source_(source), destination_(destination),
          revision_(engine.getTopology().getRoutingRevision()), metric_(engine.getRoutingMetric()),
          limit_(max_failures) {
        if (max_failures == 0 || max_failures > MAX_FAILURES)
            throw std::invalid_argument("Resilience case limit must be between 1 and 256");
        baseline_ = engine.traceRoute(source, destination);
    }

    bool RouteResilience::matches(const SimulationEngine& engine, int source,
                                  int destination) const noexcept {
        return engine_ == &engine && source_ == source && destination_ == destination &&
               revision_ == engine.getTopology().getRoutingRevision() &&
               metric_ == engine.getRoutingMetric();
    }

    bool RouteResilience::advance(const SimulationEngine& engine) {
        if (!matches(engine, source_, destination_))
            throw std::logic_error("Topology or routing metric changed; restart resilience analysis");
        if (finished())
            return false;
        const auto hop = baseline_.hops[failures_.size()];
        failures_.push_back({hop, engine.traceRoute(source_, destination_, hop.link_id)});
        return true;
    }

    std::size_t RouteResilience::criticalCount() const noexcept {
        return static_cast<std::size_t>(std::count_if(
            failures_.begin(), failures_.end(),
            [](const auto& failure) { return failure.alternative.status != RouteStatus::Reachable; }));
    }

    bool RouteResilience::finished() const noexcept {
        return baseline_.status != RouteStatus::Reachable ||
               failures_.size() >= std::min(limit_, baseline_.hops.size());
    }

    bool RouteResilience::complete() const noexcept {
        return baseline_.status == RouteStatus::Reachable &&
               failures_.size() == baseline_.hops.size();
    }
}
