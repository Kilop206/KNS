#include "intelligence/IntelligenceRequestBuilder.hpp"

#include <chrono>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>

#include "analysis/AIContextBuilder.hpp"

namespace kns::intelligence {

namespace {

std::string modeToString(
    AnalysisMode mode
)
{
    switch (mode) {
        case AnalysisMode::Quick:
            return "quick";

        case AnalysisMode::Detailed:
            return "detailed";

        case AnalysisMode::Deep:
            return "deep";
    }

    return "detailed";
}

analysis::AIContextDepth toContextDepth(
    AnalysisMode mode
)
{
    switch (mode) {
        case AnalysisMode::Quick:
            return analysis::AIContextDepth::Quick;

        case AnalysisMode::Detailed:
            return analysis::AIContextDepth::Detailed;

        case AnalysisMode::Deep:
            return analysis::AIContextDepth::Deep;
    }

    return analysis::AIContextDepth::Detailed;
}

std::string generateRequestId()
{
    const auto now =
        std::chrono::system_clock::now()
            .time_since_epoch()
            .count();

    std::random_device randomDevice;

    std::mt19937_64 generator(
        randomDevice()
    );

    std::uniform_int_distribution<
        std::uint64_t
    > distribution;

    const std::uint64_t randomValue =
        distribution(generator);

    std::ostringstream stream;

    stream
        << std::hex
        << now
        << '-'
        << randomValue;

    return stream.str();
}

} // namespace

IntelligenceRequest
IntelligenceRequestBuilder::build(
    const analysis::NetworkAnalysis& analysis,
    AnalysisMode mode
)
{
    analysis::AIContextOptions contextOptions;

    contextOptions.depth =
        toContextDepth(mode);

    switch (mode) {
        case AnalysisMode::Quick:
            contextOptions.max_critical_nodes = 3;
            contextOptions.max_critical_links = 3;
            contextOptions.max_worst_routes = 0;
            contextOptions.max_unreachable_routes = 5;
            break;

        case AnalysisMode::Detailed:
            contextOptions.max_critical_nodes = 5;
            contextOptions.max_critical_links = 5;
            contextOptions.max_worst_routes = 5;
            contextOptions.max_unreachable_routes = 10;
            break;

        case AnalysisMode::Deep:
            contextOptions.max_critical_nodes = 10;
            contextOptions.max_critical_links = 10;
            contextOptions.max_worst_routes = 10;
            contextOptions.max_unreachable_routes = 25;
            break;
    }

    IntelligenceRequest request;

    request.request_id =
        generateRequestId();

    request.mode =
        mode;

    request.context =
        analysis::AIContextBuilder::build(
            analysis,
            contextOptions
        );

    return request;
}

nlohmann::json
IntelligenceRequestBuilder::toJson(
    const IntelligenceRequest& request
)
{
    return {
        {
            "schema_version",
            "1.0"
        },
        {
            "request_id",
            request.request_id
        },
        {
            "analysis_mode",
            modeToString(request.mode)
        },
        {
            "client",
            {
                {
                    "name",
                    request.client_name
                },
                {
                    "version",
                    request.client_version
                }
            }
        },
        {
            "context",
            request.context
        }
    };
}

}