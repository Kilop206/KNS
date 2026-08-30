#pragma once

#include <vector>

namespace kns {

    class Topology;

    /// Selects the cost function used by Dijkstra when building routing tables.
    enum class RoutingMetric {
        /// Minimize end-to-end propagation delay (sum of link delay_ms). Default.
        Delay,
        /// Maximize available bandwidth (maximize minimum link bandwidth_mbps).
        Bandwidth,
        /// Minimize hop count (each link costs 1).
        HopCount,
        /// Composite: delay / bandwidth_mbps — favours low-latency, high-bandwidth paths.
        DelayBandwidth,
    };

    class Routing {
        public:
            struct DijkstraResult {
                std::vector<double> dist;
                std::vector<int> parent;
            };

            struct RoutingEntry {
                int destination = -1;
                int next_hop = -1;
                double distance = 0.0;
            };

            DijkstraResult buildDijkstra(const Topology& topology, int src,
                                          RoutingMetric metric = RoutingMetric::Delay);

            std::vector<RoutingEntry> buildRoutingTable(const Topology& topology, int src,
                                                         RoutingMetric metric = RoutingMetric::Delay);
    };

}