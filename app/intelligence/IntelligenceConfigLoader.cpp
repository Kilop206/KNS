#include "intelligence/IntelligenceConfigLoader.hpp"

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>

#include "../include/Environment.hpp"

namespace kns::app::intelligence {

namespace {

int parsePositiveInteger(
    const std::string& value,
    const char* variableName
)
{
    int result = 0;

    const char* begin =
        value.data();

    const char* end =
        value.data() + value.size();

    const auto parseResult =
        std::from_chars(
            begin,
            end,
            result
        );

    if (
        parseResult.ec != std::errc{} ||
        parseResult.ptr != end ||
        result <= 0
    ) {
        throw std::runtime_error(
            std::string(
                "Invalid value for "
            ) +
            variableName +
            ": expected a positive integer"
        );
    }

    return result;
}

void applyTimeout(
    int& target,
    const char* environmentVariable
)
{
    const auto value =
        kns::app::readEnvironmentVariable(
            environmentVariable
        );

    if (!value.has_value()) {
        return;
    }

    target =
        parsePositiveInteger(
            *value,
            environmentVariable
        );
}

} // namespace

IntelligenceClientConfig
IntelligenceConfigLoader::fromEnvironment()
{
    IntelligenceClientConfig config;

    if (
        const auto value =
            kns::app::readEnvironmentVariable(
                "KNS_INTELLIGENCE_BASE_URL"
            )
    ) {
        config.base_url =
            *value;
    }

    if (
        const auto value =
            kns::app::readEnvironmentVariable(
                "KNS_INTELLIGENCE_ENDPOINT"
            )
    ) {
        config.analyze_endpoint =
            *value;
    }

    if (
        const auto value =
            kns::app::readEnvironmentVariable(
                "KNS_INTELLIGENCE_TOKEN"
            )
    ) {
        config.bearer_token =
            *value;
    }

    applyTimeout(
        config.connection_timeout_seconds,
        "KNS_INTELLIGENCE_CONNECT_TIMEOUT"
    );

    applyTimeout(
        config.read_timeout_seconds,
        "KNS_INTELLIGENCE_READ_TIMEOUT"
    );

    applyTimeout(
        config.write_timeout_seconds,
        "KNS_INTELLIGENCE_WRITE_TIMEOUT"
    );

    return config;
}

}