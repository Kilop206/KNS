#include "analysis/AIContextBuilder.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace kns::analysis {

namespace {

std::string riskLevelToString(RiskLevel level)
{
    switch (level) {
        case RiskLevel::None:
            return "none";

        case RiskLevel::Low:
            return "low";

        case RiskLevel::Medium:
            return "medium";

        case RiskLevel::High:
            return "high";

        case RiskLevel::Critical:
            return "critical";
    }

    return "unknown";
}

std::string depthToString(AIContextDepth depth)
{
    switch (depth) {
        case AIContextDepth::Quick:
            return "quick";

        case AIContextDepth::Detailed:
            return "detailed";

        case AIContextDepth::Deep:
            return "deep";
    }

    return "detailed";
}

bool isRelevantRisk(RiskLevel level)
{
    return level == RiskLevel::Medium ||
           level == RiskLevel::High ||
           level == RiskLevel::Critical;
}

nlohmann::json nodeToContext(
    const NodeMetrics& node
)
{
    return {
        { "node_id", node.node_id },
        { "criticality_score", node.criticality_score },
        { "risk_level", riskLevelToString(node.risk_level) },
        { "degree", node.degree },
        { "route_usage_ratio", node.route_usage_ratio },
        { "articulation_point", node.articulation_point },
        { "isolated", node.isolated },
        { "reasons", node.risk_reasons }
    };
}

nlohmann::json linkToContext(
    const LinkMetrics& link
)
{
    return {
        { "from", link.from },
        { "to", link.to },
        { "risk_score", link.risk_score },
        { "risk_level", riskLevelToString(link.risk_level) },
        { "route_usage_ratio", link.route_usage_ratio },
        { "delay_ms", link.delay_ms },
        { "bandwidth_mbps", link.bandwidth_mbps },
        { "bridge", link.bridge },
        { "reasons", link.risk_reasons }
    };
}

nlohmann::json routeToContext(
    const RouteMetrics& route
)
{
    return {
        { "source", route.source },
        { "destination", route.destination },
        { "reachable", route.reachable },
        { "hop_count", route.hop_count },
        { "routing_distance", route.routing_distance },
        { "total_delay_ms", route.total_delay_ms },
        {
            "bottleneck_bandwidth_mbps",
            route.bottleneck_bandwidth_mbps
        },
        { "path", route.path }
    };
}

bool isCriticalRisk(RiskLevel level)
{
    return level == RiskLevel::Medium ||
           level == RiskLevel::High ||
           level == RiskLevel::Critical;
}

std::vector<const NodeMetrics*> selectCriticalNodes(
    const NetworkAnalysis& analysis,
    std::size_t limit
)
{
    std::vector<const NodeMetrics*> result;

    for (const auto& node : analysis.nodes) {
        if (!isCriticalRisk(node.risk_level)) {
            continue;
        }

        result.push_back(&node);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const NodeMetrics* lhs, const NodeMetrics* rhs) {
            if (lhs->criticality_score != rhs->criticality_score) {
                return lhs->criticality_score >
                       rhs->criticality_score;
            }

            return lhs->node_id < rhs->node_id;
        }
    );

    if (result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

std::vector<const LinkMetrics*> selectCriticalLinks(
    const NetworkAnalysis& analysis,
    std::size_t limit
)
{
    std::vector<const LinkMetrics*> result;

    for (const auto& link : analysis.links) {
        if (!isCriticalRisk(link.risk_level)) {
            continue;
        }

        result.push_back(&link);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const LinkMetrics* lhs, const LinkMetrics* rhs) {
            if (lhs->risk_score != rhs->risk_score) {
                return lhs->risk_score >
                       rhs->risk_score;
            }

            if (lhs->from != rhs->from) {
                return lhs->from < rhs->from;
            }

            return lhs->to < rhs->to;
        }
    );

    if (result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

std::vector<const RouteMetrics*> selectWorstRoutes(
    const NetworkAnalysis& analysis,
    std::size_t limit
)
{
    std::vector<const RouteMetrics*> result;

    for (const auto& route : analysis.routes) {
        if (!route.reachable) {
            continue;
        }

        result.push_back(&route);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const RouteMetrics* lhs, const RouteMetrics* rhs) {
            if (lhs->total_delay_ms != rhs->total_delay_ms) {
                return lhs->total_delay_ms >
                       rhs->total_delay_ms;
            }

            if (lhs->hop_count != rhs->hop_count) {
                return lhs->hop_count >
                       rhs->hop_count;
            }

            if (lhs->source != rhs->source) {
                return lhs->source < rhs->source;
            }

            return lhs->destination < rhs->destination;
        }
    );

    if (result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

std::vector<const RouteMetrics*> selectUnreachableRoutes(
    const NetworkAnalysis& analysis,
    std::size_t limit
)
{
    std::vector<const RouteMetrics*> result;

    for (const auto& route : analysis.routes) {
        if (route.reachable) {
            continue;
        }

        result.push_back(&route);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const RouteMetrics* lhs, const RouteMetrics* rhs) {
            if (lhs->source != rhs->source) {
                return lhs->source < rhs->source;
            }

            return lhs->destination < rhs->destination;
        }
    );

    if (result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

nlohmann::json buildDeterministicFindings(
    const NetworkAnalysis& analysis
)
{
    nlohmann::json findings =
        nlohmann::json::array();

    if (!analysis.connected) {
        findings.push_back({
            { "severity", "critical" },
            { "code", "NETWORK_DISCONNECTED" },
            {
                "message",
                "The network contains multiple disconnected components"
            }
        });
    }

    if (!analysis.isolated_nodes.empty()) {
        findings.push_back({
            { "severity", "critical" },
            { "code", "ISOLATED_NODES" },
            {
                "message",
                "One or more nodes are isolated from the network"
            },
            {
                "nodes",
                analysis.isolated_nodes
            }
        });
    }

    if (!analysis.articulation_points.empty()) {
        findings.push_back({
            { "severity", "high" },
            { "code", "ARTICULATION_POINTS" },
            {
                "message",
                "The topology contains articulation points"
            },
            {
                "nodes",
                analysis.articulation_points
            }
        });
    }

    if (!analysis.bridges.empty()) {
        nlohmann::json bridges =
            nlohmann::json::array();

        for (const auto& [from, to] : analysis.bridges) {
            bridges.push_back({
                { "from", from },
                { "to", to }
            });
        }

        findings.push_back({
            { "severity", "high" },
            { "code", "BRIDGES_PRESENT" },
            {
                "message",
                "The topology contains bridge links"
            },
            {
                "links",
                bridges
            }
        });
    }

    if (analysis.unreachable_route_count > 0) {
        findings.push_back({
            { "severity", "critical" },
            { "code", "UNREACHABLE_ROUTES" },
            {
                "message",
                "Some source-destination pairs are unreachable"
            },
            {
                "count",
                analysis.unreachable_route_count
            }
        });
    }

    bool mediumOrHigherRisk = false;

    for (const auto& node : analysis.nodes) {
        if (isRelevantRisk(node.risk_level)) {
            mediumOrHigherRisk = true;
            break;
        }
    }

    if (!mediumOrHigherRisk) {
        for (const auto& link : analysis.links) {
            if (isRelevantRisk(link.risk_level)) {
                mediumOrHigherRisk = true;
                break;
            }
        }
    }

    if (
        analysis.connected &&
        analysis.isolated_nodes.empty() &&
        analysis.articulation_points.empty() &&
        analysis.bridges.empty() &&
        analysis.unreachable_route_count == 0
    ) {
        findings.push_back({
            { "severity", "info" },
            { "code", "NO_STRUCTURAL_SINGLE_POINT_OF_FAILURE" },
            {
                "message",
                "No structural single points of failure were detected"
            }
        });
    }

    if (!mediumOrHigherRisk) {
        findings.push_back({
            { "severity", "info" },
            { "code", "NO_HIGH_RISK_COMPONENTS" },
            {
                "message",
                "No medium, high, or critical risk components were detected"
            }
        });
    }

    return findings;
}

} // namespace

nlohmann::json AIContextBuilder::build(
    const NetworkAnalysis& analysis,
    const AIContextOptions& options
)
{
    nlohmann::json context;

    context["schema_version"] = "1.0";
    context["context_type"] = "kns_ai_context";
    context["depth"] = depthToString(options.depth);

    context["network"] = {
        { "node_count", analysis.node_count },
        { "link_count", analysis.link_count },
        { "connected", analysis.connected },
        {
            "connected_components",
            analysis.connected_components
        }
    };

    context["routing"] = {
        {
            "reachable_routes",
            analysis.reachable_route_count
        },
        {
            "unreachable_routes",
            analysis.unreachable_route_count
        },
        {
            "average_hop_count",
            analysis.average_hop_count
        },
        {
            "maximum_hop_count",
            analysis.maximum_hop_count
        },
        {
            "average_routing_distance",
            analysis.average_routing_distance
        },
        {
            "maximum_routing_distance",
            analysis.maximum_routing_distance
        }
    };

    context["performance"] = {
        {
            "average_path_delay_ms",
            analysis.average_path_delay_ms
        },
        {
            "maximum_path_delay_ms",
            analysis.maximum_path_delay_ms
        },
        {
            "average_bottleneck_bandwidth_mbps",
            analysis.average_bottleneck_bandwidth_mbps
        },
        {
            "minimum_bottleneck_bandwidth_mbps",
            analysis.minimum_bottleneck_bandwidth_mbps
        }
    };

    context["structural_risks"] = {
        {
            "isolated_nodes",
            analysis.isolated_nodes
        },
        {
            "articulation_points",
            analysis.articulation_points
        },
        {
            "bridge_count",
            analysis.bridges.size()
        }
    };

    context["critical_nodes"] =
        nlohmann::json::array();

    for (
        const auto* node :
        selectCriticalNodes(
            analysis,
            options.max_critical_nodes
        )
    ) {
        context["critical_nodes"].push_back(
            nodeToContext(*node)
        );
    }

    context["critical_links"] =
        nlohmann::json::array();

    for (
        const auto* link :
        selectCriticalLinks(
            analysis,
            options.max_critical_links
        )
    ) {
        context["critical_links"].push_back(
            linkToContext(*link)
        );
    }

    context["findings"] =
        buildDeterministicFindings(analysis);

    if (options.depth != AIContextDepth::Quick) {
        context["worst_routes"] =
            nlohmann::json::array();

        for (
            const auto* route :
            selectWorstRoutes(
                analysis,
                options.max_worst_routes
            )
        ) {
            context["worst_routes"].push_back(
                routeToContext(*route)
            );
        }

        context["unreachable_routes"] =
            nlohmann::json::array();

        for (
            const auto* route :
            selectUnreachableRoutes(
                analysis,
                options.max_unreachable_routes
            )
        ) {
            context["unreachable_routes"].push_back(
                routeToContext(*route)
            );
        }
    }

    if (options.depth == AIContextDepth::Deep) {
        context["all_structural_bridges"] =
            nlohmann::json::array();

        for (const auto& [from, to] : analysis.bridges) {
            context["all_structural_bridges"].push_back({
                { "from", from },
                { "to", to }
            });
        }

        context["metadata"] = {
            {
                "total_routes_analyzed",
                analysis.routes.size()
            },
            {
                "total_nodes_analyzed",
                analysis.nodes.size()
            },
            {
                "total_links_analyzed",
                analysis.links.size()
            }
        };
    }

    return context;
}

}