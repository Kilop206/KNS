#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace kns {

enum class RouteStatus { Reachable, Unreachable, InvalidEndpoint, ForwardingLoop };

struct RouteHop {
    int from;
    int to;
    std::uint64_t link_id;
    double delay_ms;
    double bandwidth_mbps;
};

struct RouteTrace {
    RouteStatus status = RouteStatus::Unreachable;
    std::vector<RouteHop> hops;
    // Configuration totals for the traversed prefix, not measured performance.
    double propagation_delay_ms = 0.0;
    std::optional<double> bottleneck_mbps;
};

} // namespace kns
