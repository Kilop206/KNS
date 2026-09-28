#include "engine/core/SimulationEngine.hpp"

#include <algorithm>

namespace kns {

RouteTrace SimulationEngine::traceRoute(int source, int destination) const {
    RouteTrace result;
    const auto active = [&](int id) {
        const auto* node = topology_.getNode(id);
        return node && node->isActive();
    };
    if (!active(source) || !active(destination)) {
        result.status = RouteStatus::InvalidEndpoint;
        return result;
    }

    std::vector<bool> visited(static_cast<std::size_t>(topology_.size()), false);
    int current = source;
    visited[current] = true;
    while (current != destination) {
        const auto table = getRoutingTable(current);
        if (static_cast<std::size_t>(destination) >= table.size()) return result;
        const auto& entry = table[destination];
        if (!entry.link_id || !active(entry.next_hop)) return result;
        const auto& links = topology_.getLinksFromNode(current);
        const auto chosen = std::find_if(links.begin(), links.end(), [&](const auto& link) {
            return link && link->getId() == *entry.link_id && link->isUp() &&
                link->allowsTransmission(current, entry.next_hop);
        });
        if (chosen == links.end()) return result;
        const auto& link = **chosen;
        result.hops.push_back({current, entry.next_hop, link.getId(),
            link.getDelayMs(), link.getBandwidthMbps()});
        result.propagation_delay_ms += link.getDelayMs();
        result.bottleneck_mbps = result.bottleneck_mbps
            ? std::min(*result.bottleneck_mbps, link.getBandwidthMbps())
            : link.getBandwidthMbps();
        current = entry.next_hop;
        if (visited[current]) {
            result.status = RouteStatus::ForwardingLoop;
            return result;
        }
        visited[current] = true;
    }
    result.status = RouteStatus::Reachable;
    return result;
}

} // namespace kns
