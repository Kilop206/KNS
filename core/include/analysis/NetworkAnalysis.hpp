#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace kns::analysis {

struct LinkMetrics {
    int from = -1;
    int to = -1;

    double delay_ms = 0.0;
    double bandwidth_mbps = 0.0;

    bool bridge = false;
};

struct NodeMetrics {
    int node_id = -1;

    std::size_t degree = 0;

    bool isolated = false;
    bool articulation_point = false;
};

struct NetworkAnalysis {
    std::size_t node_count = 0;
    std::size_t link_count = 0;

    bool connected = false;

    std::size_t connected_components = 0;

    std::vector<int> isolated_nodes;
    std::vector<int> articulation_points;

    std::vector<std::pair<int, int>> bridges;

    std::vector<NodeMetrics> nodes;
    std::vector<LinkMetrics> links;
};

}