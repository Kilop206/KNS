#include "analysis/NetworkScoreCalculator.hpp"

#include <algorithm>
#include <cmath>

namespace kns::analysis {

namespace {

double clampScore(double value)
{
    return std::clamp(
        value,
        0.0,
        100.0
    );
}

double calculateResilienceScore(
    const NetworkAnalysis& analysis
)
{
    if (analysis.node_count == 0) {
        return 0.0;
    }

    double score = 100.0;

    if (!analysis.connected) {
        score -= 45.0;
    }

    if (analysis.connected_components > 1) {
        const double extraComponents =
            static_cast<double>(
                analysis.connected_components - 1
            );

        score -= std::min(
            25.0,
            extraComponents * 10.0
        );
    }

    if (!analysis.isolated_nodes.empty()) {
        const double isolatedRatio =
            static_cast<double>(
                analysis.isolated_nodes.size()
            ) /
            static_cast<double>(
                analysis.node_count
            );

        score -= 30.0 * isolatedRatio;
    }

    if (!analysis.articulation_points.empty()) {
        const double articulationRatio =
            static_cast<double>(
                analysis.articulation_points.size()
            ) /
            static_cast<double>(
                analysis.node_count
            );

        score -= 25.0 * articulationRatio;
    }

    if (!analysis.bridges.empty() &&
        analysis.link_count > 0)
    {
        const double bridgeRatio =
            static_cast<double>(
                analysis.bridges.size()
            ) /
            static_cast<double>(
                analysis.link_count
            );

        score -= 25.0 * bridgeRatio;
    }

    return clampScore(score);
}

double calculateRoutingScore(
    const NetworkAnalysis& analysis
)
{
    const std::size_t totalRoutes =
        analysis.reachable_route_count +
        analysis.unreachable_route_count;

    if (totalRoutes == 0) {
        return 100.0;
    }

    const double reachableRatio =
        static_cast<double>(
            analysis.reachable_route_count
        ) /
        static_cast<double>(
            totalRoutes
        );

    double score =
        reachableRatio * 100.0;

    if (analysis.maximum_hop_count > 0) {
        const double hopPenalty =
            std::min(
                20.0,
                analysis.average_hop_count * 2.5
            );

        score -= hopPenalty;
    }

    return clampScore(score);
}

double calculatePerformanceScore(
    const NetworkAnalysis& analysis
)
{
    double score = 100.0;

    if (analysis.maximum_path_delay_ms > 0.0) {
        const double delayPenalty =
            std::min(
                35.0,
                analysis.average_path_delay_ms *
                    0.75
            );

        score -= delayPenalty;
    }

    if (
        analysis.average_bottleneck_bandwidth_mbps > 0.0 &&
        analysis.minimum_bottleneck_bandwidth_mbps > 0.0
    ) {
        const double bandwidthRatio =
            analysis.minimum_bottleneck_bandwidth_mbps /
            analysis.average_bottleneck_bandwidth_mbps;

        const double bandwidthPenalty =
            (1.0 - std::clamp(
                bandwidthRatio,
                0.0,
                1.0
            )) * 30.0;

        score -= bandwidthPenalty;
    }

    return clampScore(score);
}

double calculateRiskScore(
    const NetworkAnalysis& analysis
)
{
    if (
        analysis.nodes.empty() &&
        analysis.links.empty()
    ) {
        return 100.0;
    }

    double totalRisk = 0.0;
    std::size_t count = 0;

    for (const auto& node : analysis.nodes) {
        totalRisk +=
            std::clamp(
                node.criticality_score,
                0.0,
                1.0
            );

        ++count;
    }

    for (const auto& link : analysis.links) {
        totalRisk +=
            std::clamp(
                link.risk_score,
                0.0,
                1.0
            );

        ++count;
    }

    if (count == 0) {
        return 100.0;
    }

    const double averageRisk =
        totalRisk /
        static_cast<double>(count);

    return clampScore(
        100.0 -
        averageRisk * 100.0
    );
}

} // namespace

NetworkScore
NetworkScoreCalculator::calculate(
    const NetworkAnalysis& analysis
)
{
    NetworkScore score;

    score.resilience_score =
        calculateResilienceScore(
            analysis
        );

    score.routing_score =
        calculateRoutingScore(
            analysis
        );

    score.performance_score =
        calculatePerformanceScore(
            analysis
        );

    score.risk_score =
        calculateRiskScore(
            analysis
        );

    score.overall_score =
        clampScore(
            score.resilience_score * 0.35 +
            score.routing_score * 0.25 +
            score.performance_score * 0.20 +
            score.risk_score * 0.20
        );

    return score;
}

}