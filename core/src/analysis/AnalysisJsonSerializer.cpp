#include "analysis/AnalysisJsonSerializer.hpp"

#include <string>

namespace kns::analysis {

namespace {

std::string riskLevelToString(
    RiskLevel level
)
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

nlohmann::json routeToJson(
    const RouteMetrics& route
)
{
    return {
        {
            "source",
            route.source
        },
        {
            "destination",
            route.destination
        },
        {
            "reachable",
            route.reachable
        },
        {
            "path",
            route.path
        },
        {
            "hop_count",
            route.hop_count
        },
        {
            "routing_distance",
            route.routing_distance
        },
        {
            "total_delay_ms",
            route.total_delay_ms
        },
        {
            "bottleneck_bandwidth_mbps",
            route.bottleneck_bandwidth_mbps
        }
    };
}

nlohmann::json nodeToJson(
    const NodeMetrics& node
)
{
    return {
        {
            "node_id",
            node.node_id
        },
        {
            "degree",
            node.degree
        },
        {
            "isolated",
            node.isolated
        },
        {
            "articulation_point",
            node.articulation_point
        },
        {
            "routes_using_node",
            node.routes_using_node
        },
        {
            "route_usage_ratio",
            node.route_usage_ratio
        },
        {
            "criticality_score",
            node.criticality_score
        },
        {
            "risk_level",
            riskLevelToString(
                node.risk_level
            )
        },
        {
            "risk_reasons",
            node.risk_reasons
        }
    };
}

nlohmann::json linkToJson(
    const LinkMetrics& link
)
{
    return {
        {
            "from",
            link.from
        },
        {
            "to",
            link.to
        },
        {
            "delay_ms",
            link.delay_ms
        },
        {
            "bandwidth_mbps",
            link.bandwidth_mbps
        },
        {
            "bridge",
            link.bridge
        },
        {
            "routes_using_link",
            link.routes_using_link
        },
        {
            "route_usage_ratio",
            link.route_usage_ratio
        },
        {
            "risk_score",
            link.risk_score
        },
        {
            "risk_level",
            riskLevelToString(
                link.risk_level
            )
        },
        {
            "risk_reasons",
            link.risk_reasons
        }
    };
}

}

nlohmann::json AnalysisJsonSerializer::toJson(
    const NetworkAnalysis& analysis
)
{
    nlohmann::json json;

    /*
     * Increment this whenever we introduce a breaking
     * change to the analysis_context structure.
     */
    json["schema_version"] = "1.0";

    json["analysis_type"] =
        "kns_network_analysis";

    /*
     * =================================================
     * Network summary
     * =================================================
     */

    json["summary"] = {
        {
            "node_count",
            analysis.node_count
        },
        {
            "link_count",
            analysis.link_count
        },
        {
            "connected",
            analysis.connected
        },
        {
            "connected_components",
            analysis.connected_components
        },
        {
            "reachable_route_count",
            analysis.reachable_route_count
        },
        {
            "unreachable_route_count",
            analysis.unreachable_route_count
        }
    };

    /*
     * =================================================
     * Structural analysis
     * =================================================
     */

    json["structure"] = {
        {
            "isolated_nodes",
            analysis.isolated_nodes
        },
        {
            "articulation_points",
            analysis.articulation_points
        },
        {
            "bridges",
            nlohmann::json::array()
        }
    };

    for (const auto& [from, to] :
         analysis.bridges)
    {
        json["structure"]["bridges"]
            .push_back({
                {
                    "from",
                    from
                },
                {
                    "to",
                    to
                }
            });
    }

    /*
     * =================================================
     * Routing summary
     * =================================================
     */

    json["routing"] = {
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

    /*
     * =================================================
     * Physical path metrics
     * =================================================
     */

    json["path_metrics"] = {
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

    /*
     * =================================================
     * Nodes
     * =================================================
     */

    json["nodes"] =
        nlohmann::json::array();

    for (const auto& node :
         analysis.nodes)
    {
        json["nodes"].push_back(
            nodeToJson(node)
        );
    }

    /*
     * =================================================
     * Links
     * =================================================
     */

    json["links"] =
        nlohmann::json::array();

    for (const auto& link :
         analysis.links)
    {
        json["links"].push_back(
            linkToJson(link)
        );
    }

    /*
     * =================================================
     * Routes
     * =================================================
     */

    json["routes"] =
        nlohmann::json::array();

    for (const auto& route :
         analysis.routes)
    {
        json["routes"].push_back(
            routeToJson(route)
        );
    }

    return json;
}

}