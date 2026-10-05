#include <catch2/catch_test_macros.hpp>

#include "analysis/AIContextBuilder.hpp"
#include "analysis/AnalysisJsonSerializer.hpp"
#include "analysis/NetworkAnalyzer.hpp"
#include "network/Topology.hpp"

using namespace kns;

TEST_CASE("Network analysis exposes queue-management facts to intelligence context", "[analysis][aqm][intelligence]")
{
    Topology topology(2);
    auto link = topology.addLinkPtr(0, 1, 10.0, 5.0, 0.0, LinkMode::FULL_DUPLEX, 16);
    link->configureRed(4, 12, 0.25);

    const auto analysis = analysis::NetworkAnalyzer{}.analyze(topology);
    REQUIRE(analysis.links.size() == 1);

    const auto& metrics = analysis.links.front();
    REQUIRE(metrics.queue_capacity == 16);
    REQUIRE(metrics.queue_policy == "red");
    REQUIRE(metrics.red_min_threshold == 4);
    REQUIRE(metrics.red_max_threshold == 12);
    REQUIRE(metrics.red_max_drop_probability == 0.25);

    analysis::AIContextOptions options;
    options.depth = analysis::AIContextDepth::Deep;
    const auto context = analysis::AIContextBuilder::build(analysis, options);

    REQUIRE(context.at("critical_links").is_array());
    REQUIRE(context.at("critical_links").size() == 1);
    const auto& linkContext = context.at("critical_links").at(0);
    REQUIRE(linkContext.at("queue_capacity") == 16);
    REQUIRE(linkContext.at("queue_policy") == "red");
    REQUIRE(linkContext.at("red_min_threshold") == 4);
    REQUIRE(linkContext.at("red_max_threshold") == 12);
    REQUIRE(linkContext.at("red_max_drop_probability") == 0.25);

    const auto serialized = analysis::AnalysisJsonSerializer::toJson(analysis);
    const auto& serializedLink = serialized.at("links").at(0);
    REQUIRE(serializedLink.at("queue_policy") == "red");
    REQUIRE(serializedLink.at("queue_capacity") == 16);
}

TEST_CASE("Drop-tail analysis uses explicit defaults", "[analysis][aqm]")
{
    Topology topology(2);
    topology.addLink(0, 1, 100.0, 1.0);

    const auto analysis = analysis::NetworkAnalyzer{}.analyze(topology);
    REQUIRE(analysis.links.size() == 1);
    const auto& metrics = analysis.links.front();

    REQUIRE(metrics.queue_policy == "drop_tail");
    REQUIRE(metrics.queue_capacity == 32);
    REQUIRE(metrics.red_min_threshold == 0);
    REQUIRE(metrics.red_max_threshold == 0);
    REQUIRE(metrics.red_max_drop_probability == 0.0);
}
