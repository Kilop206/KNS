#include "intelligence/IntelligenceResponseParser.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace kns::intelligence {

namespace {

FindingSeverity parseSeverity(
    const std::string& value
)
{
    if (value == "info") {
        return FindingSeverity::Info;
    }

    if (value == "low") {
        return FindingSeverity::Low;
    }

    if (value == "medium") {
        return FindingSeverity::Medium;
    }

    if (value == "high") {
        return FindingSeverity::High;
    }

    if (value == "critical") {
        return FindingSeverity::Critical;
    }

    throw std::runtime_error(
        "Unknown finding severity: " + value
    );
}

IntelligenceStatus parseStatus(
    const std::string& value
)
{
    if (value == "completed") {
        return IntelligenceStatus::Completed;
    }

    if (value == "failed") {
        return IntelligenceStatus::Failed;
    }

    if (value == "partial") {
        return IntelligenceStatus::Partial;
    }

    throw std::runtime_error(
        "Unknown intelligence status: " + value
    );
}

void requireField(
    const nlohmann::json& json,
    const char* field
)
{
    if (!json.contains(field)) {
        throw std::runtime_error(
            std::string(
                "Missing required field: "
            ) +
            field
        );
    }
}

void requireString(
    const nlohmann::json& json,
    const char* field
)
{
    requireField(json, field);

    if (!json.at(field).is_string()) {
        throw std::runtime_error(
            std::string(
                "Field must be a string: "
            ) +
            field
        );
    }
}

void requireNumber(
    const nlohmann::json& json,
    const char* field
)
{
    requireField(json, field);

    if (!json.at(field).is_number()) {
        throw std::runtime_error(
            std::string(
                "Field must be numeric: "
            ) +
            field
        );
    }
}

IntelligenceAffectedLink parseAffectedLink(
    const nlohmann::json& json
)
{
    if (!json.is_object()) {
        throw std::runtime_error(
            "Affected link must be an object"
        );
    }

    requireField(json, "from");
    requireField(json, "to");

    if (
        !json.at("from").is_number_integer() ||
        !json.at("to").is_number_integer()
    ) {
        throw std::runtime_error(
            "Affected link endpoints must be integers"
        );
    }

    IntelligenceAffectedLink link;

    link.from =
        json.at("from").get<int>();

    link.to =
        json.at("to").get<int>();

    return link;
}

IntelligenceFinding parseFinding(
    const nlohmann::json& json
)
{
    if (!json.is_object()) {
        throw std::runtime_error(
            "Finding must be an object"
        );
    }

    requireString(json, "id");
    requireString(json, "severity");
    requireString(json, "category");
    requireString(json, "title");
    requireString(json, "description");

    IntelligenceFinding finding;

    finding.id =
        json.at("id").get<std::string>();

    finding.severity =
        parseSeverity(
            json.at("severity")
                .get<std::string>()
        );

    finding.category =
        json.at("category")
            .get<std::string>();

    finding.title =
        json.at("title")
            .get<std::string>();

    finding.description =
        json.at("description")
            .get<std::string>();

    if (json.contains("affected_nodes")) {
        if (!json.at("affected_nodes").is_array()) {
            throw std::runtime_error(
                "affected_nodes must be an array"
            );
        }

        for (
            const auto& node :
            json.at("affected_nodes")
        ) {
            if (!node.is_number_integer()) {
                throw std::runtime_error(
                    "affected_nodes values must be integers"
                );
            }

            finding.affected_nodes.push_back(
                node.get<int>()
            );
        }
    }

    if (json.contains("affected_links")) {
        if (!json.at("affected_links").is_array()) {
            throw std::runtime_error(
                "affected_links must be an array"
            );
        }

        for (
            const auto& link :
            json.at("affected_links")
        ) {
            finding.affected_links.push_back(
                parseAffectedLink(link)
            );
        }
    }

    return finding;
}

IntelligenceRecommendation parseRecommendation(
    const nlohmann::json& json
)
{
    if (!json.is_object()) {
        throw std::runtime_error(
            "Recommendation must be an object"
        );
    }

    requireString(json, "id");
    requireString(json, "priority");
    requireString(json, "title");
    requireString(json, "description");

    IntelligenceRecommendation recommendation;

    recommendation.id =
        json.at("id")
            .get<std::string>();

    recommendation.priority =
        json.at("priority")
            .get<std::string>();

    recommendation.title =
        json.at("title")
            .get<std::string>();

    recommendation.description =
        json.at("description")
            .get<std::string>();

    return recommendation;
}

} // namespace

IntelligenceResponse
IntelligenceResponseParser::parse(
    const nlohmann::json& json
)
{
    if (!json.is_object()) {
        throw std::runtime_error(
            "Intelligence response must be a JSON object"
        );
    }

    requireString(
        json,
        "schema_version"
    );

    requireString(
        json,
        "analysis_id"
    );

    requireString(
        json,
        "status"
    );

    requireNumber(
        json,
        "network_score"
    );

    requireString(
        json,
        "summary"
    );

    IntelligenceResponse response;

    response.schema_version =
        json.at("schema_version")
            .get<std::string>();

    if (response.schema_version != "1.0") {
        throw std::runtime_error(
            "Unsupported intelligence response schema: " +
            response.schema_version
        );
    }

    response.analysis_id =
        json.at("analysis_id")
            .get<std::string>();

    response.status =
        parseStatus(
            json.at("status")
                .get<std::string>()
        );

    response.network_score =
        json.at("network_score")
            .get<double>();

    if (
        response.network_score < 0.0 ||
        response.network_score > 100.0
    ) {
        throw std::runtime_error(
            "network_score must be between 0 and 100"
        );
    }

    response.summary =
        json.at("summary")
            .get<std::string>();

    if (json.contains("findings")) {
        if (!json.at("findings").is_array()) {
            throw std::runtime_error(
                "findings must be an array"
            );
        }

        for (
            const auto& finding :
            json.at("findings")
        ) {
            response.findings.push_back(
                parseFinding(finding)
            );
        }
    }

    if (json.contains("recommendations")) {
        if (
            !json.at("recommendations")
                .is_array()
        ) {
            throw std::runtime_error(
                "recommendations must be an array"
            );
        }

        for (
            const auto& recommendation :
            json.at("recommendations")
        ) {
            response.recommendations.push_back(
                parseRecommendation(
                    recommendation
                )
            );
        }
    }

    return response;
}

IntelligenceResponse
IntelligenceResponseParser::parse(
    const std::string& jsonText
)
{
    try {
        const auto json =
            nlohmann::json::parse(
                jsonText
            );

        return parse(json);
    }
    catch (
        const nlohmann::json::parse_error& e
    ) {
        throw std::runtime_error(
            std::string(
                "Invalid intelligence JSON: "
            ) +
            e.what()
        );
    }
}

}