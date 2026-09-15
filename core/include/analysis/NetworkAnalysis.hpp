#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace kns::analysis {

struct RouteMetrics {
    int source = -1;
    int destination = -1;

    bool reachable = false;

    double routing_distance = 0.0;

    std::size_t hop_count = 0;

    std::vector<int> path;

    double total_delay_ms = 0.0;

    double bottleneck_bandwidth_mbps = 0.0;
};

struct LinkMetrics {
    int from = -1;
    int to = -1;

    double delay_ms = 0.0;
    double bandwidth_mbps = 0.0;

    bool bridge = false;

    std::size_t routes_using_link = 0;

    double route_usage_ratio = 0.0;
};

struct NodeMetrics {
    int node_id = -1;

    std::size_t degree = 0;

    bool isolated = false;
    bool articulation_point = false;

    std::size_t routes_using_node = 0;

    double route_usage_ratio = 0.0;
};

struct NetworkAnalysis {
    std::size_t node_count = 0;
    std::size_t link_count = 0;

    bool connected = false;

    std::size_t connected_components = 0;

    std::size_t reachable_route_count = 0;
    std::size_t unreachable_route_count = 0;

    double average_hop_count = 0.0;
    std::size_t maximum_hop_count = 0;

    double average_routing_distance = 0.0;
    double maximum_routing_distance = 0.0;

    double average_path_delay_ms = 0.0;
    double maximum_path_delay_ms = 0.0;

    double average_bottleneck_bandwidth_mbps = 0.0;
    double minimum_bottleneck_bandwidth_mbps = 0.0;

    std::vector<int> isolated_nodes;
    std::vector<int> articulation_points;

    std::vector<std::pair<int, int>> bridges;

    std::vector<NodeMetrics> nodes;
    std::vector<LinkMetrics> links;

    std::vector<RouteMetrics> routes;
};

}