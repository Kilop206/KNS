#include "analysis/NetworkAnalyzer.hpp"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "network/Topology.hpp"

namespace kns::analysis {

namespace {

struct Graph {
    std::vector<std::vector<int>> adjacency;
    std::set<std::pair<int, int>> edges;
};

std::pair<int, int> normalizeEdge(int a, int b)
{
    if (a > b) {
        std::swap(a, b);
    }

    return {a, b};
}

Graph buildGraph(const Topology& topology)
{
    const int node_count = topology.size();

    Graph graph;

    graph.adjacency.resize(
        static_cast<std::size_t>(node_count)
    );

    for (int node = 0; node < node_count; ++node) {

        const auto& links = topology.getLinksFromNode(node);

        for (const auto& link : links) {

            if (!link) {
                continue;
            }

            const int a = link->getA();
            const int b = link->getB();

            if (a < 0 ||
                b < 0 ||
                a >= node_count ||
                b >= node_count)
            {
                continue;
            }

            if (a == b) {
                continue;
            }

            const auto edge = normalizeEdge(a, b);

            if (!graph.edges.insert(edge).second) {
                continue;
            }

            graph.adjacency[a].push_back(b);
            graph.adjacency[b].push_back(a);
        }
    }

    return graph;
}

void dfsComponent(
    int node,
    const Graph& graph,
    std::vector<bool>& visited
)
{
    visited[node] = true;

    for (const int neighbor : graph.adjacency[node]) {

        if (!visited[neighbor]) {
            dfsComponent(
                neighbor,
                graph,
                visited
            );
        }
    }
}

struct TarjanResult {
    std::set<int> articulation_points;

    std::set<std::pair<int, int>> bridges;
};

void tarjanDfs(
    int node,
    int parent,
    const Graph& graph,
    std::vector<bool>& visited,
    std::vector<int>& discovery,
    std::vector<int>& low,
    int& timer,
    TarjanResult& result
)
{
    visited[node] = true;

    discovery[node] = timer;
    low[node] = timer;

    ++timer;

    int children = 0;

    for (const int neighbor : graph.adjacency[node]) {

        if (neighbor == parent) {
            continue;
        }

        if (visited[neighbor]) {

            low[node] = std::min(
                low[node],
                discovery[neighbor]
            );

            continue;
        }

        ++children;

        tarjanDfs(
            neighbor,
            node,
            graph,
            visited,
            discovery,
            low,
            timer,
            result
        );

        low[node] = std::min(
            low[node],
            low[neighbor]
        );

        // Bridge
        if (low[neighbor] > discovery[node]) {

            result.bridges.insert(
                normalizeEdge(node, neighbor)
            );
        }

        // Articulation point
        if (parent != -1 &&
            low[neighbor] >= discovery[node])
        {
            result.articulation_points.insert(node);
        }
    }

    // Root articulation rule
    if (parent == -1 && children > 1) {
        result.articulation_points.insert(node);
    }
}

TarjanResult findCriticalElements(
    const Graph& graph
)
{
    const std::size_t n = graph.adjacency.size();

    std::vector<bool> visited(n, false);
    std::vector<int> discovery(n, -1);
    std::vector<int> low(n, -1);

    int timer = 0;

    TarjanResult result;

    for (std::size_t node = 0; node < n; ++node) {

        if (visited[node]) {
            continue;
        }

        tarjanDfs(
            static_cast<int>(node),
            -1,
            graph,
            visited,
            discovery,
            low,
            timer,
            result
        );
    }

    return result;
}

} // namespace

NetworkAnalysis NetworkAnalyzer::analyze(
    const Topology& topology
) const
{
    NetworkAnalysis analysis;

    const int node_count = topology.size();

    analysis.node_count =
        static_cast<std::size_t>(
            std::max(node_count, 0)
        );

    if (node_count <= 0) {
        return analysis;
    }

    const Graph graph = buildGraph(topology);

    analysis.link_count = graph.edges.size();

    // -------------------------------------------------
    // Components
    // -------------------------------------------------

    std::vector<bool> visited(
        static_cast<std::size_t>(node_count),
        false
    );

    std::size_t component_count = 0;

    for (int node = 0; node < node_count; ++node) {

        if (visited[node]) {
            continue;
        }

        ++component_count;

        dfsComponent(
            node,
            graph,
            visited
        );
    }

    analysis.connected_components =
        component_count;

    analysis.connected =
        component_count == 1;

    // -------------------------------------------------
    // Critical elements
    // -------------------------------------------------

    const TarjanResult critical =
        findCriticalElements(graph);

    analysis.articulation_points.assign(
        critical.articulation_points.begin(),
        critical.articulation_points.end()
    );

    analysis.bridges.assign(
        critical.bridges.begin(),
        critical.bridges.end()
    );

    // -------------------------------------------------
    // Nodes
    // -------------------------------------------------

    analysis.nodes.reserve(
        static_cast<std::size_t>(node_count)
    );

    for (int node = 0; node < node_count; ++node) {

        NodeMetrics metrics;

        metrics.node_id = node;

        metrics.degree =
            graph.adjacency[node].size();

        metrics.isolated =
            metrics.degree == 0;

        metrics.articulation_point =
            critical.articulation_points.contains(node);

        if (metrics.isolated) {
            analysis.isolated_nodes.push_back(node);
        }

        analysis.nodes.push_back(metrics);
    }

    // -------------------------------------------------
    // Links
    // -------------------------------------------------

    analysis.links.reserve(graph.edges.size());

    for (const auto& [a, b] : graph.edges) {

        LinkMetrics metrics;

        metrics.from = a;
        metrics.to = b;

        metrics.bridge =
            critical.bridges.contains(
                normalizeEdge(a, b)
            );

        /*
         * Find the real KNS Link so that we can attach
         * its physical metrics to this analysis.
         */

        for (const auto& link :
             topology.getLinksFromNode(a))
        {
            if (!link) {
                continue;
            }

            const auto endpoints =
                normalizeEdge(
                    link->getA(),
                    link->getB()
                );

            if (endpoints != normalizeEdge(a, b)) {
                continue;
            }

            metrics.delay_ms =
                link->getDelayMs();

            metrics.bandwidth_mbps =
                link->getBandwidthMbps();

            break;
        }

        analysis.links.push_back(metrics);
    }

    return analysis;
}

}