#include "analysis/NetworkAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "intelligence/IntelligenceRequestBuilder.hpp"
#include "network/Routing.hpp"
#include "network/Topology.hpp"

namespace kns::analysis {

namespace {

struct Graph {
    std::vector<std::vector<int>> adjacency;
    std::set<std::pair<int, int>> edges;
};

using RoutingTables =
    std::vector<std::vector<Routing::RoutingEntry>>;

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

    if (node_count <= 0) {
        return graph;
    }

    graph.adjacency.resize(
        static_cast<std::size_t>(node_count)
    );

    for (int node = 0; node < node_count; ++node) {

        const auto& links =
            topology.getLinksFromNode(node);

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

            const auto edge =
                normalizeEdge(a, b);

            /*
             * getLinksFromNode() can expose the same
             * physical link from both endpoints.
             *
             * The set prevents duplicate graph edges.
             */
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

    for (const int neighbor :
         graph.adjacency[node])
    {
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

    for (const int neighbor :
         graph.adjacency[node])
    {
        if (neighbor == parent) {
            continue;
        }

        /*
         * Back edge.
         */
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

        /*
         * If the subtree rooted at neighbor cannot
         * reach an ancestor of node, the edge is a
         * bridge.
         */
        if (low[neighbor] > discovery[node]) {

            result.bridges.insert(
                normalizeEdge(
                    node,
                    neighbor
                )
            );
        }

        /*
         * Non-root articulation point.
         */
        if (parent != -1 &&
            low[neighbor] >= discovery[node])
        {
            result.articulation_points.insert(
                node
            );
        }
    }

    /*
     * Root node is an articulation point only when
     * it has more than one DFS child.
     */
    if (parent == -1 &&
        children > 1)
    {
        result.articulation_points.insert(
            node
        );
    }
}

TarjanResult findCriticalElements(
    const Graph& graph
)
{
    const std::size_t node_count =
        graph.adjacency.size();

    std::vector<bool> visited(
        node_count,
        false
    );

    std::vector<int> discovery(
        node_count,
        -1
    );

    std::vector<int> low(
        node_count,
        -1
    );

    int timer = 0;

    TarjanResult result;

    for (std::size_t node = 0;
         node < node_count;
         ++node)
    {
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

RoutingTables buildAllRoutingTables(
    const Topology& topology
)
{
    const int node_count =
        topology.size();

    RoutingTables tables;

    if (node_count <= 0) {
        return tables;
    }

    tables.resize(
        static_cast<std::size_t>(
            node_count
        )
    );

    Routing routing;

    for (int source = 0;
         source < node_count;
         ++source)
    {
        tables[source] =
            routing.buildRoutingTable(
                topology,
                source
            );
    }

    return tables;
}

RouteMetrics buildRoute(
    int source,
    int destination,
    const RoutingTables& tables,
    int node_count
)
{
    RouteMetrics route;

    route.source = source;
    route.destination = destination;

    if (source < 0 ||
        destination < 0 ||
        source >= node_count ||
        destination >= node_count)
    {
        return route;
    }

    if (source == destination) {

        route.reachable = true;
        route.routing_distance = 0.0;

        route.path.push_back(
            source
        );

        return route;
    }

    if (source >=
        static_cast<int>(
            tables.size()
        ))
    {
        return route;
    }

    const auto& source_table =
        tables[source];

    if (destination >=
        static_cast<int>(
            source_table.size()
        ))
    {
        return route;
    }

    const auto& destination_entry =
        source_table[destination];

    /*
     * KNS already represents unreachable routing
     * entries using infinity.
     */
    if (!std::isfinite(
            destination_entry.distance))
    {
        return route;
    }

    route.routing_distance =
        destination_entry.distance;

    std::vector<bool> visited(
        static_cast<std::size_t>(
            node_count
        ),
        false
    );

    int current = source;

    route.path.push_back(
        current
    );

    /*
     * Reconstruct the path using each node's routing
     * table.
     *
     * The step limit and visited array protect the
     * analyzer against malformed routing tables or
     * routing loops.
     */
    for (int steps = 0;
         steps < node_count;
         ++steps)
    {
        if (current == destination) {
            break;
        }

        if (current < 0 ||
            current >= node_count)
        {
            route.path.clear();
            return route;
        }

        if (visited[current]) {

            route.path.clear();

            return route;
        }

        visited[current] = true;

        if (current >=
            static_cast<int>(
                tables.size()
            ))
        {
            route.path.clear();
            return route;
        }

        const auto& current_table =
            tables[current];

        if (destination >=
            static_cast<int>(
                current_table.size()
            ))
        {
            route.path.clear();
            return route;
        }

        const int next =
            current_table[destination]
                .next_hop;

        if (next < 0 ||
            next >= node_count ||
            next == current)
        {
            route.path.clear();
            return route;
        }

        route.path.push_back(
            next
        );

        current = next;
    }

    if (current != destination) {

        route.path.clear();

        return route;
    }

    route.reachable = true;

    if (route.path.size() >= 2) {

        route.hop_count =
            route.path.size() - 1;
    }

    return route;
}

void countNodeRouteUsage(
    NetworkAnalysis& analysis
)
{
    for (const auto& route :
         analysis.routes)
    {
        if (!route.reachable ||
            route.path.size() <= 2)
        {
            continue;
        }

        /*
         * Source and destination are deliberately
         * excluded.
         *
         * We want transit dependency, not simple
         * participation in a route.
         */
        for (std::size_t i = 1;
             i + 1 < route.path.size();
             ++i)
        {
            const int node =
                route.path[i];

            if (node < 0 ||
                node >= static_cast<int>(
                    analysis.nodes.size()
                ))
            {
                continue;
            }

            ++analysis
                .nodes[node]
                .routes_using_node;
        }
    }
}

void countLinkRouteUsage(
    NetworkAnalysis& analysis
)
{
    std::map<
        std::pair<int, int>,
        std::size_t
    > link_index;

    for (std::size_t i = 0;
         i < analysis.links.size();
         ++i)
    {
        const auto& link =
            analysis.links[i];

        link_index[
            normalizeEdge(
                link.from,
                link.to
            )
        ] = i;
    }

    for (const auto& route :
         analysis.routes)
    {
        if (!route.reachable ||
            route.path.size() < 2)
        {
            continue;
        }

        for (std::size_t i = 0;
             i + 1 < route.path.size();
             ++i)
        {
            const auto edge =
                normalizeEdge(
                    route.path[i],
                    route.path[i + 1]
                );

            const auto it =
                link_index.find(edge);

            if (it ==
                link_index.end())
            {
                continue;
            }

            ++analysis
                .links[it->second]
                .routes_using_link;
        }
    }
}

void calculateRouteUsageRatios(
    NetworkAnalysis& analysis
)
{
    const double total_routes =
        static_cast<double>(
            analysis.reachable_route_count
        );

    if (total_routes <= 0.0) {
        return;
    }

    for (auto& node :
         analysis.nodes)
    {
        node.route_usage_ratio =
            static_cast<double>(
                node.routes_using_node
            ) /
            total_routes;
    }

    for (auto& link :
         analysis.links)
    {
        link.route_usage_ratio =
            static_cast<double>(
                link.routes_using_link
            ) /
            total_routes;
    }
}

using Edge = std::pair<int, int>;

using LinkMetricIndex =
    std::map<Edge, std::size_t>;

LinkMetricIndex buildLinkMetricIndex(
    const NetworkAnalysis& analysis
)
{
    LinkMetricIndex index;

    for (std::size_t i = 0;
         i < analysis.links.size();
         ++i)
    {
        const auto& link =
            analysis.links[i];

        index[
            normalizeEdge(
                link.from,
                link.to
            )
        ] = i;
    }

    return index;
}

} // namespace

void calculateRoutePhysicalMetrics(
    NetworkAnalysis& analysis
)
{
    const LinkMetricIndex link_index =
        buildLinkMetricIndex(analysis);

    for (auto& route : analysis.routes) {

        if (!route.reachable ||
            route.path.size() < 2)
        {
            continue;
        }

        double total_delay = 0.0;

        double bottleneck_bandwidth =
            std::numeric_limits<double>::infinity();

        bool valid = true;

        for (std::size_t i = 0;
             i + 1 < route.path.size();
             ++i)
        {
            const Edge edge =
                normalizeEdge(
                    route.path[i],
                    route.path[i + 1]
                );

            const auto it =
                link_index.find(edge);

            if (it == link_index.end()) {
                valid = false;
                break;
            }

            const auto& link =
                analysis.links[it->second];

            total_delay +=
                link.delay_ms;

            bottleneck_bandwidth =
                std::min(
                    bottleneck_bandwidth,
                    link.bandwidth_mbps
                );
        }

        if (!valid) {
            continue;
        }

        route.total_delay_ms =
            total_delay;

        if (std::isfinite(
                bottleneck_bandwidth))
        {
            route.bottleneck_bandwidth_mbps =
                bottleneck_bandwidth;
        }
    }
}

void calculateGlobalPathMetrics(
    NetworkAnalysis& analysis
)
{
    double total_delay = 0.0;
    double total_bandwidth = 0.0;

    std::size_t valid_routes = 0;

    double minimum_bandwidth =
        std::numeric_limits<double>::infinity();

    for (const auto& route :
         analysis.routes)
    {
        if (!route.reachable ||
            route.path.size() < 2)
        {
            continue;
        }

        ++valid_routes;

        total_delay +=
            route.total_delay_ms;

        total_bandwidth +=
            route.bottleneck_bandwidth_mbps;

        analysis.maximum_path_delay_ms =
            std::max(
                analysis.maximum_path_delay_ms,
                route.total_delay_ms
            );

        if (route.bottleneck_bandwidth_mbps > 0.0) {

            minimum_bandwidth =
                std::min(
                    minimum_bandwidth,
                    route.bottleneck_bandwidth_mbps
                );
        }
    }

    if (valid_routes == 0) {
        return;
    }

    const double count =
        static_cast<double>(
            valid_routes
        );

    analysis.average_path_delay_ms =
        total_delay / count;

    analysis.average_bottleneck_bandwidth_mbps =
        total_bandwidth / count;

    if (std::isfinite(minimum_bandwidth)) {

        analysis.minimum_bottleneck_bandwidth_mbps =
            minimum_bandwidth;
    }
}



double clamp01(double value)
{
    return std::clamp(
        value,
        0.0,
        1.0
    );
}

RiskLevel classifyRisk(double score)
{
    if (score >= 0.80) {
        return RiskLevel::Critical;
    }

    if (score >= 0.60) {
        return RiskLevel::High;
    }

    if (score >= 0.35) {
        return RiskLevel::Medium;
    }

    if (score > 0.0) {
        return RiskLevel::Low;
    }

    return RiskLevel::None;
}

void calculateNodeCriticality(
    NetworkAnalysis& analysis
)
{
    std::size_t minimum_degree =
        std::numeric_limits<std::size_t>::max();

    std::size_t maximum_degree = 0;

    for (const auto& node : analysis.nodes) {
        minimum_degree =
            std::min(
                minimum_degree,
                node.degree
            );

        maximum_degree =
            std::max(
                maximum_degree,
                node.degree
            );
    }

    for (auto& node : analysis.nodes) {

        const double usage_score =
            clamp01(
                node.route_usage_ratio
            );

        const double articulation_score =
            node.articulation_point
                ? 1.0
                : 0.0;

        double degree_score = 0.0;

        if (maximum_degree > minimum_degree) {
            degree_score =
                static_cast<double>(
                    node.degree - minimum_degree
                ) /
                static_cast<double>(
                    maximum_degree - minimum_degree
                );
        }

        const double isolation_score =
            node.isolated
                ? 1.0
                : 0.0;

        node.criticality_score =
            usage_score * 0.35 +
            articulation_score * 0.35 +
            degree_score * 0.20 +
            isolation_score * 0.10;

        node.criticality_score =
            clamp01(
                node.criticality_score
            );

        node.risk_level =
            classifyRisk(
                node.criticality_score
            );

        if (node.articulation_point) {

            node.risk_reasons.emplace_back(
                "Node is an articulation point"
            );
        }

        if (node.route_usage_ratio >= 0.50) {

            node.risk_reasons.emplace_back(
                "Node carries at least 50% of reachable routes"
            );
        }

        if (degree_score >= 0.75) {

            node.risk_reasons.emplace_back(
                "Node has high relative connectivity"
            );
        }

        if (node.isolated) {

            node.risk_reasons.emplace_back(
                "Node is isolated from the network"
            );
        }
    }
}

void calculateLinkRisk(
    NetworkAnalysis& analysis
)
{
    double maximum_delay = 0.0;
    double maximum_bandwidth = 0.0;

    for (const auto& link :
         analysis.links)
    {
        maximum_delay =
            std::max(
                maximum_delay,
                link.delay_ms
            );

        maximum_bandwidth =
            std::max(
                maximum_bandwidth,
                link.bandwidth_mbps
            );
    }

    for (auto& link :
         analysis.links)
    {
        const double usage_score =
            clamp01(
                link.route_usage_ratio
            );

        const double bridge_score =
            link.bridge
                ? 1.0
                : 0.0;

        double delay_score = 0.0;

        if (maximum_delay > 0.0) {

            delay_score =
                link.delay_ms /
                maximum_delay;
        }

        double bandwidth_risk = 0.0;

        if (maximum_bandwidth > 0.0) {

            const double normalized =
                link.bandwidth_mbps /
                maximum_bandwidth;

            bandwidth_risk =
                1.0 -
                clamp01(normalized);
        }

        link.risk_score =
            usage_score * 0.45 +
            bridge_score * 0.35 +
            bandwidth_risk * 0.10 +
            delay_score * 0.10;

        link.risk_score =
            clamp01(
                link.risk_score
            );

        link.risk_level =
            classifyRisk(
                link.risk_score
            );

        if (link.bridge) {

            link.risk_reasons.emplace_back(
                "Link is a bridge and its failure can partition the network"
            );
        }

        if (link.route_usage_ratio >= 0.50) {

            link.risk_reasons.emplace_back(
                "Link carries at least 50% of reachable routes"
            );
        }

        if (bandwidth_risk >= 0.75) {

            link.risk_reasons.emplace_back(
                "Link bandwidth is low relative to the topology"
            );
        }

        if (delay_score >= 0.75) {

            link.risk_reasons.emplace_back(
                "Link delay is high relative to the topology"
            );
        }
    }
}

NetworkAnalysis NetworkAnalyzer::analyze(
    const Topology& topology
) const
{
    NetworkAnalysis analysis;

    const int node_count =
        topology.size();

    analysis.node_count =
        static_cast<std::size_t>(
            std::max(
                node_count,
                0
            )
        );

    if (node_count <= 0) {
        return analysis;
    }

    /*
     * =================================================
     * Physical graph
     * =================================================
     */

    const Graph graph =
        buildGraph(topology);

    analysis.link_count =
        graph.edges.size();

    /*
     * =================================================
     * Connected components
     * =================================================
     */

    std::vector<bool> visited(
        static_cast<std::size_t>(
            node_count
        ),
        false
    );

    std::size_t component_count = 0;

    for (int node = 0;
         node < node_count;
         ++node)
    {
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

    /*
     * =================================================
     * Bridges and articulation points
     * =================================================
     */

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

    /*
     * =================================================
     * Node metrics
     * =================================================
     */

    analysis.nodes.reserve(
        static_cast<std::size_t>(
            node_count
        )
    );

    for (int node = 0;
         node < node_count;
         ++node)
    {
        NodeMetrics metrics;

        metrics.node_id = node;

        metrics.degree =
            graph.adjacency[node].size();

        metrics.isolated =
            metrics.degree == 0;

        metrics.articulation_point =
            critical
                .articulation_points
                .contains(node);

        if (metrics.isolated) {

            analysis
                .isolated_nodes
                .push_back(node);
        }

        analysis.nodes.push_back(
            metrics
        );
    }

    /*
     * =================================================
     * Link metrics
     * =================================================
     */

    analysis.links.reserve(
        graph.edges.size()
    );

    for (const auto& [a, b] :
         graph.edges)
    {
        LinkMetrics metrics;

        metrics.from = a;
        metrics.to = b;

        metrics.bridge =
            critical.bridges.contains(
                normalizeEdge(a, b)
            );

        /*
         * Find the actual KNS Link so that its
         * configured metrics can be included in the
         * deterministic analysis.
         */
        const auto& links =
            topology.getLinksFromNode(a);

        for (const auto& link : links) {

            if (!link) {
                continue;
            }

            const auto endpoints =
                normalizeEdge(
                    link->getA(),
                    link->getB()
                );

            if (endpoints !=
                normalizeEdge(a, b))
            {
                continue;
            }

            metrics.delay_ms =
                link->getDelayMs();

            metrics.bandwidth_mbps =
                link->getBandwidthMbps();

            break;
        }

        analysis.links.push_back(
            metrics
        );
    }

    /*
     * =================================================
     * Routing analysis
     * =================================================
     */

    const RoutingTables routing_tables =
        buildAllRoutingTables(topology);

    analysis.routes.reserve(
        static_cast<std::size_t>(
            node_count *
            std::max(node_count - 1, 0)
        )
    );

    double total_hops = 0.0;
    double total_distance = 0.0;

    for (int source = 0;
         source < node_count;
         ++source)
    {
        for (int destination = 0;
             destination < node_count;
             ++destination)
        {
            if (source == destination) {
                continue;
            }

            RouteMetrics route =
                buildRoute(
                    source,
                    destination,
                    routing_tables,
                    node_count
                );

            if (route.reachable) {

                ++analysis
                    .reachable_route_count;

                total_hops +=
                    static_cast<double>(
                        route.hop_count
                    );

                total_distance +=
                    route.routing_distance;

                analysis.maximum_hop_count =
                    std::max(
                        analysis.maximum_hop_count,
                        route.hop_count
                    );

                analysis.maximum_routing_distance =
                    std::max(
                        analysis.maximum_routing_distance,
                        route.routing_distance
                    );
            }
            else {

                ++analysis
                    .unreachable_route_count;
            }

            analysis.routes.push_back(
                std::move(route)
            );
        }
    }

    /*
     * =================================================
     * Global route metrics
     * =================================================
     */

    if (analysis.reachable_route_count > 0) {

        const double reachable =
            static_cast<double>(
                analysis.reachable_route_count
            );

        analysis.average_hop_count =
            total_hops /
            reachable;

        analysis.average_routing_distance =
            total_distance /
            reachable;
    }

    /*
     * =================================================
     * Dependency analysis
     * =================================================
     */

    countNodeRouteUsage(
        analysis
    );

    countLinkRouteUsage(
        analysis
    );

    calculateRouteUsageRatios(
        analysis
    );

    calculateRoutePhysicalMetrics(
        analysis
    );

    calculateGlobalPathMetrics(
        analysis
    );

    calculateNodeCriticality(
        analysis
    );

    calculateLinkRisk(
        analysis
    );

    return analysis;
}

} // namespace kns::analysis