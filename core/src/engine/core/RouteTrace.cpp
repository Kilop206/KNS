#include "engine/core/SimulationEngine.hpp"

#include <algorithm>

namespace kns {

    RouteTrace SimulationEngine::traceRoute(int source, int destination,
                                           std::optional<std::uint64_t> excluded_link) const {
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
            if (current != source &&
                !deviceCapabilities(topology_.getNode(current)->getDeviceInfo().type).forward)
                return result;
            Routing::RoutingEntry entry;
            if (excluded_link) {
                // Recover only the requested destination's next hop. Building a
                // full table here would traverse every destination's parent chain.
                const auto tree = Routing{}.buildDijkstra(topology_, current, routing_metric_,
                                                          excluded_link);
                int next = destination;
                while (tree.parent[next] != current) {
                    next = tree.parent[next];
                    if (next < 0)
                        return result;
                }
                entry.next_hop = next;
                entry.link_id = tree.parent_link[next];
            } else {
                const auto table = getRoutingTable(current);
                if (static_cast<std::size_t>(destination) >= table.size())
                    return result;
                entry = table[destination];
            }
            if (!entry.link_id || !active(entry.next_hop))
                return result;
            const auto& links = topology_.getLinksFromNode(current);
            const auto chosen = std::find_if(links.begin(), links.end(), [&](const auto& link) {
                return link && link->getId() == *entry.link_id && link->isUp() &&
                       link->allowsTransmission(current, entry.next_hop);
            });
            if (chosen == links.end())
                return result;
            const auto& link = **chosen;
            result.hops.push_back({current, entry.next_hop, link.getId(), link.getDelayMs(),
                                   link.getBandwidthMbps()});
            result.propagation_delay_ms += link.getDelayMs();
            result.bottleneck_mbps =
                result.bottleneck_mbps ? std::min(*result.bottleneck_mbps, link.getBandwidthMbps())
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
